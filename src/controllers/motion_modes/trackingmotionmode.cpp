#include "trackingmotionmode.h"
#include "controllers/gimbalcontroller.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QDateTime>
#include <QtGlobal>
#include <cmath>

// ============================================================================
// OPTIMIZED TRACKING MOTION MODE FOR AZD-KD SERVO (50ms UPDATE CYCLE)
// ============================================================================
// Key fixes:
// 1. Added derivative term for damping (prevents overshoot)
// 2. Removed software rate limiting (let hardware accel handle it)
// 3. Reduced filter time constants (faster response)
// 4. Proper feed-forward that doesn't fight PID
// ============================================================================

TrackingMotionMode::TrackingMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent)
    , m_targetValid(false)
    , m_targetAz(0.0), m_targetEl(0.0)
    , m_smoothedAzVel_dps(0.0), m_smoothedElVel_dps(0.0)
    , m_previousDesiredAzVel(0.0), m_previousDesiredElVel(0.0)
    , m_lastGimbalAz(0.0), m_lastGimbalEl(0.0)
    , m_gimbalVelAz(0.0), m_gimbalVelEl(0.0)
    , m_lastSentAzCmd(0.0), m_lastSentElCmd(0.0)
{
    // -------------------------------------------------------------------------
    // TUNED PID GAINS FOR 50ms UPDATE CYCLE + FILTERED dErr
    // -------------------------------------------------------------------------
    // v5: Reduced Kd because dErr filter adds ~50ms phase lag
    //     Using filtered derivative-on-error for noise immunity

    // Azimuth
    m_azPid.Kp = 1;     // Slightly higher P for faster response
    m_azPid.Kd = 0.35;    // Reduced D (filter adds lag, raw dErr was too noisy)
    m_azPid.Ki = 0.0;     // OFF for tracking
    m_azPid.maxIntegral = 3.0;

    // Elevation
    m_elPid.Kp = 1.2;
    m_elPid.Kd = 0.4;
    m_elPid.Ki = 0.0;
    m_elPid.maxIntegral = 3.0;
    
    // Initialize measurement velocity tracking
    m_lastGimbalAz = 0.0;
    m_lastGimbalEl = 0.0;
}

void TrackingMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[TrackingMotionMode] Enter - P+D+FF Control (v6 - Filtered dErr + Filtered FF)";

    m_targetValid = false;
    m_azPid.reset();
    m_elPid.reset();

    m_previousDesiredAzVel = 0.0;
    m_previousDesiredElVel = 0.0;
    m_gimbalVelAz = 0.0;
    m_gimbalVelEl = 0.0;
    m_lastValidAzRate = 0.0;
    m_lastValidElRate = 0.0;
    m_logCycleCounter = 0;

    // Reset derivative-on-error state (prevents initial kick)
    m_prevErrAz = 0.0;
    m_prevErrEl = 0.0;
    m_filteredDErrAz = 0.0;
    m_filteredDErrEl = 0.0;

    // Reset feedforward filter state
    m_filteredTargetVelAz = 0.0;
    m_filteredTargetVelEl = 0.0;

    // Initialize LAC state machine
    m_state = LACTrackingState::TRACK;
    m_lacBlendFactor = 0.0;
    m_manualAzOffset_deg = 0.0;
    m_manualElOffset_deg = 0.0;

    startVelocityTimer();


    // Capture initial gimbal position for velocity estimation
    if (controller && controller->systemStateModel()) {
        SystemStateData data = controller->systemStateModel()->data();
        m_lastGimbalAz = data.gimbalAz;
        m_lastGimbalEl = data.gimbalEl;
    }
}

void TrackingMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[TrackingMotionMode] Exit";
    stopServos(controller);
}
 

void TrackingMotionMode::onTargetPositionUpdated(
    double imageErrAz_deg,
    double imageErrEl_deg,
    double targetVelAz_dps,
    double targetVelEl_dps,
    bool isValid)
{
    m_targetValid = isValid;

    if (!isValid) {
        m_imageErrAz = 0.0;
        m_imageErrEl = 0.0;
        m_smoothedAzVel_dps = 0.0;
        m_smoothedElVel_dps = 0.0;
        return;
    }

    // Image-space error (deg)
    m_imageErrAz = imageErrAz_deg;
    m_imageErrEl = imageErrEl_deg;

    // Target angular velocity (deg/s)
    m_smoothedAzVel_dps = targetVelAz_dps;
    m_smoothedElVel_dps = targetVelEl_dps;
}

void TrackingMotionMode::updateImpl(GimbalController* controller, double dt)
{
    // NOTE: Base class updateWithSafety() has already verified SafetyInterlock.canMove()
    // This method is only called after general safety checks pass.

    // =========================================================================
    // 0. MODE-SPECIFIC VALIDATION
    // =========================================================================
    if (!controller || !m_targetValid || dt <= 0.0001)
        return;

    SystemStateData data = controller->systemStateModel()->data();

    // Mode-specific safety: tracking requires dead man switch
    if (!data.deadManSwitchActive) {
        sendStabilizedServoCommands(controller, 0.0, 0.0, false, dt);
        return;
    }

    // =========================================================================
    // 1. STATE MACHINE & BLENDING (TRACK / FIRE_LEAD / RECENTER)
    // =========================================================================
    // Physical constraint: RIGID CRADLE - camera and gun are locked together
    // Consequence: During lead injection, target MUST drift off-center
    //
    // State transitions:
    //   TRACK -> FIRE_LEAD  : when lacArmed && leadAngleCompensationActive
    //   FIRE_LEAD -> RECENTER : when LAC deactivated
    //   RECENTER -> TRACK   : when blend factor reaches 0
    // =========================================================================
    LACTrackingState prevState = m_state;

    if (data.lacArmed && data.deadReckoningActive) {
        m_state = LACTrackingState::FIRE_LEAD;
    } else {
        if (prevState == LACTrackingState::FIRE_LEAD) {
            m_state = LACTrackingState::RECENTER;
        } else if (m_lacBlendFactor <= 0.001) {
            m_state = LACTrackingState::TRACK;
        }
        // else: stay in RECENTER while ramping down
    }

    // Update blend factor based on state
    if (m_state == LACTrackingState::FIRE_LEAD) {
        // Ramp IN lead (~200ms to reach 1.0)
        m_lacBlendFactor = qMin(1.0, m_lacBlendFactor + dt * LAC_BLEND_RAMP_IN);

        // SAFETY: manual offsets forbidden during fire - clear immediately
        m_manualAzOffset_deg = 0.0;
        m_manualElOffset_deg = 0.0;
    } else {
        // Ramp OUT lead (~333ms to reach 0.0)
        m_lacBlendFactor = qMax(0.0, m_lacBlendFactor - dt * LAC_BLEND_RAMP_OUT);
    }

    // Log state transitions
    if (prevState != m_state) {
        const char* stateNames[] = {"TRACK", "FIRE_LEAD", "RECENTER"};
        qDebug() << "[TrackingMotionMode] State:" << stateNames[static_cast<int>(prevState)]
                 << "->" << stateNames[static_cast<int>(m_state)]
                 << "blend=" << QString::number(m_lacBlendFactor, 'f', 2);
    }

    // =========================================================================
    // 2. INPUTS
    // =========================================================================
    double errAz = m_imageErrAz;
    double errEl = m_imageErrEl;

    // =========================================================================
    // 2a. FILTERED DERIVATIVE-ON-ERROR (Noise-resistant damping)
    // =========================================================================
    // PROBLEM IDENTIFIED: Raw dErr amplifies tracker noise → command spikes →
    // gimbal jerks → image blur → more tracker noise → positive feedback!
    //
    // SOLUTION: Filter dErr with IIR low-pass + hard clamp to prevent spikes
    // This provides damping synchronized with camera while rejecting noise.

    double rawDErrAz = 0.0;
    double rawDErrEl = 0.0;
    if (dt > 1e-4) {
        rawDErrAz = (errAz - m_prevErrAz) / dt;
        rawDErrEl = (errEl - m_prevErrEl) / dt;
    }
    m_prevErrAz = errAz;
    m_prevErrEl = errEl;

    // Low-pass filter on dErr (tau = 0.1s → ~10Hz cutoff, rejects tracker jitter)
    constexpr double DERR_FILTER_TAU = 0.1;  // 100ms time constant
    double alpha = (dt > 0) ? (1.0 - std::exp(-dt / DERR_FILTER_TAU)) : 0.0;
    m_filteredDErrAz = alpha * rawDErrAz + (1.0 - alpha) * m_filteredDErrAz;
    m_filteredDErrEl = alpha * rawDErrEl + (1.0 - alpha) * m_filteredDErrEl;

    // Hard clamp to prevent tracker glitches from causing huge D-term spikes
    constexpr double MAX_DERR = 8.0;  // deg/s - max believable error rate
    double dErrAz = qBound(-MAX_DERR, m_filteredDErrAz, MAX_DERR);
    double dErrEl = qBound(-MAX_DERR, m_filteredDErrEl, MAX_DERR);

    // =========================================================================
    // 3. MANUAL NUDGE (POSITION OFFSET - TRACK STATE ONLY)
    // =========================================================================
    constexpr double JOYSTICK_DB = 0.15;
    constexpr double MANUAL_GAIN = 4.0;
    constexpr double MAX_MANUAL  = 5.0;
    constexpr double DECAY       = 2.0;
    constexpr double ERR_DB      = 0.05;
    // REMOVED: constexpr double RATE_DB = 0.8; // DEADBAND DEATH SPIRAL - was causing divergent oscillations!

    bool manualActive =
        std::abs(data.joystickAzValue) > JOYSTICK_DB ||
        std::abs(data.joystickElValue) > JOYSTICK_DB;

    // Manual input ONLY when lead influence is negligible
    if (manualActive && m_lacBlendFactor < 0.1) {
        m_manualAzOffset_deg -= data.joystickAzValue * MANUAL_GAIN * dt;
        m_manualElOffset_deg -= data.joystickElValue * MANUAL_GAIN * dt;

        m_manualAzOffset_deg = qBound(-MAX_MANUAL, m_manualAzOffset_deg, MAX_MANUAL);
        m_manualElOffset_deg = qBound(-MAX_MANUAL, m_manualElOffset_deg, MAX_MANUAL);
    } else if (m_state != LACTrackingState::FIRE_LEAD) {
        // Smooth decay (not during FIRE_LEAD - already zeroed above)
        m_manualAzOffset_deg -= m_manualAzOffset_deg * DECAY * dt;
        m_manualElOffset_deg -= m_manualElOffset_deg * DECAY * dt;

        if (std::abs(m_manualAzOffset_deg) < ERR_DB) m_manualAzOffset_deg = 0.0;
        if (std::abs(m_manualElOffset_deg) < ERR_DB) m_manualElOffset_deg = 0.0;
    }

    // =========================================================================
    // 4. TRACKING CONTROL (P + D + FEEDFORWARD)
    // =========================================================================
    // STABILITY FIX: Removed RATE_DB deadband - was causing "Deadband Death Spiral"
    // When rate dropped below 0.8 deg/s, damping vanished → undamped P-control → divergent oscillation

    // Apply small error deadband (prevents hunting around zero)
    double effectiveErrAz = errAz + m_manualAzOffset_deg;
    double effectiveErrEl = errEl + m_manualElOffset_deg;

    if (std::abs(effectiveErrAz) < ERR_DB) effectiveErrAz = 0.0;
    if (std::abs(effectiveErrEl) < ERR_DB) effectiveErrEl = 0.0;

    // REMOVED: Rate deadband was the PRIMARY cause of oscillation!
    // The 0.8 deg/s threshold created an undamped zone where P-only control oscillated.

    // -------------------------------------------------------------------------
    // FEEDFORWARD: Use target velocity to eliminate "chasing" lag
    // -------------------------------------------------------------------------
    // STABILITY FIX v6: Raw FF was causing divergence!
    // When tracker glitched, m_smoothedAzVel_dps spiked → huge FF → divergence
    //
    // SOLUTION:
    // 1. Filter FF velocity (same tau as dErr filter)
    // 2. Clamp FF to reasonable max (±3 deg/s)
    // 3. Disable FF when error is large (FF is for fine tracking, not recovery)
    // 4. Reduce FF_GAIN to 0.5 (partial velocity matching)

    constexpr double FF_GAIN = 0.5;  // Reduced from 1.0 - partial velocity match
    constexpr double FF_FILTER_TAU = 0.15;  // 150ms time constant
    constexpr double MAX_FF = 3.0;  // Max FF contribution (deg/s)
    constexpr double FF_ERROR_THRESHOLD = 0.15;  // Disable FF when |error| > 0.15°

    // Filter the target velocity (reject tracker glitches)
    double ffAlpha = (dt > 0) ? (1.0 - std::exp(-dt / FF_FILTER_TAU)) : 0.0;
    m_filteredTargetVelAz = ffAlpha * m_smoothedAzVel_dps + (1.0 - ffAlpha) * m_filteredTargetVelAz;
    m_filteredTargetVelEl = ffAlpha * m_smoothedElVel_dps + (1.0 - ffAlpha) * m_filteredTargetVelEl;

    // Compute FF with gain and clamp
    double ffAz = qBound(-MAX_FF, m_filteredTargetVelAz * FF_GAIN, MAX_FF);
    double ffEl = qBound(-MAX_FF, m_filteredTargetVelEl * FF_GAIN, MAX_FF);

    // Disable FF when error is large (we're in recovery mode, not steady tracking)
    if (std::abs(effectiveErrAz) > FF_ERROR_THRESHOLD) ffAz = 0.0;
    if (std::abs(effectiveErrEl) > FF_ERROR_THRESHOLD) ffEl = 0.0;

    // -------------------------------------------------------------------------
    // P + D CONTROL (Derivative on Error - synchronized with camera)
    // -------------------------------------------------------------------------
    // CRITICAL: Using dErr/dt (computed from image error) instead of servo RPM
    // Servo RPM has 100-200ms Modbus latency → turns damping into positive feedback!
    // dErr is synchronized with camera → proper phase relationship for damping
    //
    // Control Law:  cmd = Kp*error + Kd*dError + FF
    //   - Kp drives toward target
    //   - Kd damps when error is decreasing (prevents overshoot)
    //   - FF matches target velocity (eliminates chase lag)
    double trackAzCmd =
        m_azPid.Kp * effectiveErrAz +
        m_azPid.Kd * dErrAz +
        ffAz;

    double trackElCmd =
        m_elPid.Kp * effectiveErrEl +
        m_elPid.Kd * dErrEl +
        ffEl;

    // =========================================================================
    // 5. LEAD INJECTION (OPEN-LOOP VELOCITY - PHYSICS BASED)
    // =========================================================================
    // IMPORTANT FOR RIGID CRADLE:
    // Lead does NOT attempt to re-center the target.
    // It intentionally drives ahead and lets the image drift.
    // This is CORRECT behavior - the gun must aim ahead of the target.
    double lacAzCmd = data.lacLatchedAzRate_dps * LAC_RATE_BIAS_GAIN;
    double lacElCmd = data.lacLatchedElRate_dps * LAC_RATE_BIAS_GAIN;
    // =========================================================================
    // 6. FINAL BLEND
    // =========================================================================
    // lacBlendFactor = 0.0 : 100% tracking control (target centered)
    // lacBlendFactor = 1.0 : 100% lead injection (target drifts)
    double finalAzCmd =
        (1.0 - m_lacBlendFactor) * trackAzCmd +
        m_lacBlendFactor         * lacAzCmd;

    double finalElCmd =
        (1.0 - m_lacBlendFactor) * trackElCmd +
        m_lacBlendFactor         * lacElCmd;

    // =========================================================================
    // 7. SATURATION & OUTPUT
    // =========================================================================
    constexpr double MAX_VEL_AZ = 12.0;
    constexpr double MAX_VEL_EL = 10.0;

    finalAzCmd = qBound(-MAX_VEL_AZ, finalAzCmd, MAX_VEL_AZ);
    finalElCmd = qBound(-MAX_VEL_EL, finalElCmd, MAX_VEL_EL);

    // =========================================================================
    // 8. DEBUG OUTPUT (Enhanced with P+D+FF breakdown)
    // =========================================================================
    if (++m_logCycleCounter >= 10) {
        m_logCycleCounter = 0;

        const char* stateNames[] = {"TRACK", "FIRE_LEAD", "RECENTER"};

        // Compute individual control terms for debugging
        double pTermAz = m_azPid.Kp * effectiveErrAz;
        double dTermAz = m_azPid.Kd * dErrAz;

        qDebug().nospace()
            << "[TRK] "
            << "state=" << stateNames[static_cast<int>(m_state)] << " "
            << "err(" << QString::number(errAz, 'f', 2).toStdString().c_str() << ", "
                      << QString::number(errEl, 'f', 2).toStdString().c_str() << ") "
            << "P=" << QString::number(pTermAz, 'f', 2).toStdString().c_str() << " "
            << "D=" << QString::number(dTermAz, 'f', 2).toStdString().c_str() << " "
            << "FF=" << QString::number(ffAz, 'f', 2).toStdString().c_str() << " "
            << "cmd(" << QString::number(trackAzCmd, 'f', 2).toStdString().c_str() << ", "
                      << QString::number(trackElCmd, 'f', 2).toStdString().c_str() << ") "
            << "final(" << QString::number(finalAzCmd, 'f', 2).toStdString().c_str() << ", "
                        << QString::number(finalElCmd, 'f', 2).toStdString().c_str() << ")";
    }

    // =========================================================================
    // 9. SEND SERVO COMMANDS (sign convention per system requirement)
    // =========================================================================

    constexpr double CMD_CHANGE_THRESHOLD = 0.05;  // deg/s
    if (std::abs(finalAzCmd - m_lastSentAzCmd) < CMD_CHANGE_THRESHOLD &&
        std::abs(finalElCmd - m_lastSentElCmd) < CMD_CHANGE_THRESHOLD) {
        return;  // Don't flood Modbus with identical commands
    }
    m_lastSentAzCmd = finalAzCmd;
    m_lastSentElCmd = finalElCmd;

    sendStabilizedServoCommands(controller, -finalAzCmd, -finalElCmd, false, dt);
}
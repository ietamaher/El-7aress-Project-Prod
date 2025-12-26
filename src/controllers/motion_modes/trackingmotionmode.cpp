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
{
    // -------------------------------------------------------------------------
    // TUNED PID GAINS FOR 50ms UPDATE CYCLE
    // -------------------------------------------------------------------------
    // These override config file for field testing - move to config once tuned
    
    // Azimuth
    m_azPid.Kp = 0.45;
    m_azPid.Kd = 0.3;    // Much higher damping
    m_azPid.Ki = 0.0;     // OFF for tracking
    m_azPid.maxIntegral = 3.0;

    // Elevation
    m_elPid.Kp = 0.5;
    m_elPid.Kd = 0.35;
    m_elPid.Ki = 0.0;
    m_elPid.maxIntegral = 3.0;
    
    // Initialize measurement velocity tracking
    m_lastGimbalAz = 0.0;
    m_lastGimbalEl = 0.0;
}

void TrackingMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[TrackingMotionMode] Enter - P+D+FF Control (Stability Fix v4)";

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

    // Initialize LAC state machine
    m_state = TrackingState::TRACK;
    m_lacBlendFactor = 0.0;
    m_manualAzOffset_deg = 0.0;
    m_manualElOffset_deg = 0.0;

    startVelocityTimer();

    if (controller) {
        // =====================================================================
        // CRITICAL: Set hardware acceleration HIGH
        // =====================================================================
        // AZD-KD can handle 1,000,000+ Hz/s easily
        // With 568 steps/deg, 100000 Hz/s ≈ 176 deg/s² (very responsive)
        // This lets the servo reach commanded velocity FAST
        // Software PID handles the control - hardware handles the ramp

        if (auto azServo = controller->azimuthServo()) {
            setAcceleration(azServo, 100000);  // 100 kHz/s
        }
        if (auto elServo = controller->elevationServo()) {
            setAcceleration(elServo, 100000);  // 100 kHz/s
        }
    }

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

void TrackingMotionMode::update(GimbalController* controller, double dt)
{
    // =========================================================================
    // 0. SAFETY & VALIDATION
    // =========================================================================
    if (!controller || !m_targetValid || dt <= 0.0001)
        return;

    SystemStateData data = controller->systemStateModel()->data();

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
    TrackingState prevState = m_state;

    if (data.lacArmed && data.deadReckoningActive) {
        m_state = TrackingState::FIRE_LEAD;
    } else {
        if (prevState == TrackingState::FIRE_LEAD) {
            m_state = TrackingState::RECENTER;
        } else if (m_lacBlendFactor <= 0.001) {
            m_state = TrackingState::TRACK;
        }
        // else: stay in RECENTER while ramping down
    }

    // Update blend factor based on state
    if (m_state == TrackingState::FIRE_LEAD) {
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
    // 2a. DERIVATIVE-ON-ERROR (Synchronized with camera, not laggy servo RPM)
    // =========================================================================
    // CRITICAL FIX: Servo RPM feedback has 100-200ms Modbus latency, which
    // turns damping into positive feedback at 20Hz update rate.
    // Instead, compute derivative from image error change - synchronized with camera.
    double dErrAz = 0.0;
    double dErrEl = 0.0;
    if (dt > 1e-4) {
        dErrAz = (errAz - m_prevErrAz) / dt;
        dErrEl = (errEl - m_prevErrEl) / dt;
    }
    m_prevErrAz = errAz;
    m_prevErrEl = errEl;

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
    } else if (m_state != TrackingState::FIRE_LEAD) {
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
    // Without FF: gimbal always lags behind moving target (Kp must "stretch" error)
    // With FF: gimbal anticipates target motion → near-zero steady-state error
    constexpr double FF_GAIN = 1.0;  // 1.0 = full velocity matching
    double ffAz = m_smoothedAzVel_dps * FF_GAIN;
    double ffEl = m_smoothedElVel_dps * FF_GAIN;

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
    sendStabilizedServoCommands(controller, -finalAzCmd, -finalElCmd, false, dt);
}
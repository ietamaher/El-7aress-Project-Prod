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
    qDebug() << "[TrackingMotionMode] Enter - Rigid Cradle LAC v3";

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

    double currentAzRate_dps = data.azRpm * 6.0;
    double currentElRate_dps = data.elRpm * 6.0;

    // =========================================================================
    // 3. MANUAL NUDGE (POSITION OFFSET - TRACK STATE ONLY)
    // =========================================================================
    constexpr double JOYSTICK_DB = 0.15;
    constexpr double MANUAL_GAIN = 4.0;
    constexpr double MAX_MANUAL  = 5.0;
    constexpr double DECAY       = 2.0;
    constexpr double ERR_DB      = 0.05;
    constexpr double RATE_DB     = 0.8;

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
    // 4. TRACKING CONTROL (PURE VELOCITY LOOP - P + D)
    // =========================================================================
    // Apply deadbands
    double effectiveErrAz = errAz + m_manualAzOffset_deg;
    double effectiveErrEl = errEl + m_manualElOffset_deg;

    if (std::abs(effectiveErrAz) < ERR_DB) effectiveErrAz = 0.0;
    if (std::abs(effectiveErrEl) < ERR_DB) effectiveErrEl = 0.0;

    if (std::abs(currentAzRate_dps) < RATE_DB) currentAzRate_dps = 0.0;
    if (std::abs(currentElRate_dps) < RATE_DB) currentElRate_dps = 0.0;

    // P + D (single damping source from measured rate)
    double trackAzCmd =
        m_azPid.Kp * effectiveErrAz -
        m_azPid.Kd * currentAzRate_dps;

    double trackElCmd =
        m_elPid.Kp * effectiveErrEl -
        m_elPid.Kd * currentElRate_dps;

    // =========================================================================
    // 5. LEAD INJECTION (OPEN-LOOP VELOCITY - PHYSICS BASED)
    // =========================================================================
    // IMPORTANT FOR RIGID CRADLE:
    // Lead does NOT attempt to re-center the target.
    // It intentionally drives ahead and lets the image drift.
    // This is CORRECT behavior - the gun must aim ahead of the target.
    double lacAzCmd = data.lacLatchedAzRate_dps * LAC_RATE_BIAS_GAIN;
    double lacElCmd = data.lacLatchedElRate_dps * LAC_RATE_BIAS_GAIN;
    qDebug() << "LAC latched rates:"
            << data.lacLatchedAzRate_dps
            << data.lacLatchedElRate_dps;
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
    // 8. DEBUG OUTPUT
    // =========================================================================
    if (++m_logCycleCounter >= 10) {
        m_logCycleCounter = 0;

        const char* stateNames[] = {"TRACK", "FIRE_LEAD", "RECENTER"};
        qDebug().nospace()
            << "[TRK] "
            << "state=" << stateNames[static_cast<int>(m_state)] << " "
            << "blend=" << QString::number(m_lacBlendFactor, 'f', 2).toStdString().c_str() << " "
            << "err(" << QString::number(errAz, 'f', 2).toStdString().c_str() << ", "
                      << QString::number(errEl, 'f', 2).toStdString().c_str() << ") "
            << "track(" << QString::number(trackAzCmd, 'f', 2).toStdString().c_str() << ", "
                        << QString::number(trackElCmd, 'f', 2).toStdString().c_str() << ") "
            << "lac(" << QString::number(lacAzCmd, 'f', 2).toStdString().c_str() << ", "
                      << QString::number(lacElCmd, 'f', 2).toStdString().c_str() << ") "
            << "final(" << QString::number(finalAzCmd, 'f', 2).toStdString().c_str() << ", "
                        << QString::number(finalElCmd, 'f', 2).toStdString().c_str() << ")";
    }

    // =========================================================================
    // 9. SEND SERVO COMMANDS (sign convention per system requirement)
    // =========================================================================
    sendStabilizedServoCommands(controller, -finalAzCmd, -finalElCmd, false, dt);
}
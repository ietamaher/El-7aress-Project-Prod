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
    qDebug() << "[TrackingMotionMode] Enter - Optimized v2";
    
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
    if (!controller || !m_targetValid || dt <= 0.0001)
        return;

    SystemStateData data = controller->systemStateModel()->data();

    // ------------------------------------------------------------------
    // SAFETY
    // ------------------------------------------------------------------
    if (!data.deadManSwitchActive) {
        sendStabilizedServoCommands(controller, 0.0, 0.0, false, dt);
        return;
    }

    // ------------------------------------------------------------------
    // INPUTS
    // ------------------------------------------------------------------
    double errAz = m_imageErrAz;
    double errEl = m_imageErrEl;

    double currentAzRate_dps = data.azRpm * 6.0;
    double currentElRate_dps = data.elRpm * 6.0;

    // ------------------------------------------------------------------
    // DEADBANDS (RATE + ERROR)
    // ------------------------------------------------------------------
    constexpr double ERR_DB  = 0.05;
    constexpr double RATE_DB = 0.8;

    if (std::abs(errAz) < ERR_DB) errAz = 0.0;
    if (std::abs(errEl) < ERR_DB) errEl = 0.0;

    if (std::abs(currentAzRate_dps) < RATE_DB) currentAzRate_dps = 0.0;
    if (std::abs(currentElRate_dps) < RATE_DB) currentElRate_dps = 0.0;

    // ------------------------------------------------------------------
    // MANUAL NUDGE (UNCHANGED LOGIC)
    // ------------------------------------------------------------------
    constexpr double JOYSTICK_DB = 0.15;
    constexpr double MANUAL_GAIN = 4.0;
    constexpr double MAX_MANUAL_OFFSET = 5.0;
    constexpr double DECAY_SPEED = 2.0;

    if (data.lacArmed && data.leadAngleCompensationActive) {
        m_manualAzOffset_deg = 0.0;
        m_manualElOffset_deg = 0.0;
    }

    bool manualActive =
        std::abs(data.joystickAzValue) > JOYSTICK_DB ||
        std::abs(data.joystickElValue) > JOYSTICK_DB;

    if (manualActive && !(data.lacArmed && data.leadAngleCompensationActive)) {
        m_manualAzOffset_deg += data.joystickAzValue * MANUAL_GAIN * dt;
        m_manualElOffset_deg += data.joystickElValue * MANUAL_GAIN * dt;

        m_manualAzOffset_deg = qBound(-MAX_MANUAL_OFFSET, m_manualAzOffset_deg, MAX_MANUAL_OFFSET);
        m_manualElOffset_deg = qBound(-MAX_MANUAL_OFFSET, m_manualElOffset_deg, MAX_MANUAL_OFFSET);
    } else {
        m_manualAzOffset_deg -= m_manualAzOffset_deg * DECAY_SPEED * dt;
        m_manualElOffset_deg -= m_manualElOffset_deg * DECAY_SPEED * dt;

        if (std::abs(m_manualAzOffset_deg) < ERR_DB) m_manualAzOffset_deg = 0.0;
        if (std::abs(m_manualElOffset_deg) < ERR_DB) m_manualElOffset_deg = 0.0;
    }

    // ------------------------------------------------------------------
    // EFFECTIVE ERROR (IMAGE + MANUAL)
    // ------------------------------------------------------------------
    double effectiveErrAz = errAz + m_manualAzOffset_deg;
    double effectiveErrEl = errEl + m_manualElOffset_deg;

    if (std::abs(effectiveErrAz) < ERR_DB) effectiveErrAz = 0.0;
    if (std::abs(effectiveErrEl) < ERR_DB) effectiveErrEl = 0.0;

    // ------------------------------------------------------------------
    // VELOCITY PID (SINGLE DAMPING SOURCE)
    // ------------------------------------------------------------------
    double azVelCmd = m_azPid.Kp * effectiveErrAz
                    - m_azPid.Kd * currentAzRate_dps;

    double elVelCmd = m_elPid.Kp * effectiveErrEl
                    - m_elPid.Kd * currentElRate_dps;

    // ------------------------------------------------------------------
    // LAC OVERRIDE (FULL PRIORITY)
    // ------------------------------------------------------------------
    if (data.lacArmed && data.leadAngleCompensationActive) {
        azVelCmd += data.lacLatchedAzRate_dps * LAC_RATE_BIAS_GAIN;
        elVelCmd += data.lacLatchedElRate_dps * LAC_RATE_BIAS_GAIN;
    }

    // ------------------------------------------------------------------
    // SATURATION
    // ------------------------------------------------------------------
    constexpr double MAX_VEL_AZ = 12.0;
    constexpr double MAX_VEL_EL = 10.0;

    azVelCmd = qBound(-MAX_VEL_AZ, azVelCmd, MAX_VEL_AZ);
    elVelCmd = qBound(-MAX_VEL_EL, elVelCmd, MAX_VEL_EL);

    // ------------------------------------------------------------------
    // DEBUG (UNCHANGED FORMAT)
    // ------------------------------------------------------------------
    if (++m_logCycleCounter >= 10) {
        m_logCycleCounter = 0;
        qDebug().nospace()
            << "[TRK] "
            << "err(" << errAz << ", " << errEl << ") "
            << "eff(" << effectiveErrAz << ", " << effectiveErrEl << ") "
            << "rate(" << currentAzRate_dps << ", " << currentElRate_dps << ") "
            << "cmd(" << azVelCmd << ", " << elVelCmd << ") "
            << "nudge(" << m_manualAzOffset_deg << ", " << m_manualElOffset_deg << ") "
            << "LAC:" << (data.leadAngleCompensationActive ? "ON" : "off");
    }

    // ------------------------------------------------------------------
    // OUTPUT (SIGN KEPT AS YOUR SYSTEM EXPECTS)
    // ------------------------------------------------------------------
    sendStabilizedServoCommands(controller, -azVelCmd, -elVelCmd, false, dt);
}

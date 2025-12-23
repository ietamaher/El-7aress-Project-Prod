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
    m_azPid.Kp = 0.6;     // 4× faster than 0.15
    m_azPid.Kd = 0.12;    // 4× 0.03 → keeps damping
    m_azPid.Ki = 0.0;     // OFF for tracking
    m_azPid.maxIntegral = 3.0;

    // Elevation
    m_elPid.Kp = 0.7;
    m_elPid.Kd = 0.14;
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

    startVelocityTimer();

    if (controller) {
        // =====================================================================
        // CRITICAL: Set hardware acceleration HIGH
        // =====================================================================
        // AZD-KD can handle 1,000,000+ Hz/s easily
        // With 568 steps/deg, 500000 Hz/s ≈ 880 deg/s² (very responsive)
        // This lets the servo reach commanded velocity FAST
        // Software PID handles the control - hardware handles the ramp
        
        if (auto azServo = controller->azimuthServo()) {
            setAcceleration(azServo, 500000);  // 500 kHz/s
        }
        if (auto elServo = controller->elevationServo()) {
            setAcceleration(elServo, 500000);  // 500 kHz/s  
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
    if (!controller || !m_targetValid || dt <= 0.0)
        return;

    SystemStateData data = controller->systemStateModel()->data();

    if (!data.deadManSwitchActive) {
        sendStabilizedServoCommands(controller, 0.0, 0.0, false, dt);
        return;
    }

    // ----------------------------------------------------
    // IMAGE-SPACE ERROR (ONLY SOURCE OF ERROR)
    // ----------------------------------------------------
    double errAz = m_imageErrAz;   // deg
    double errEl = m_imageErrEl;   // deg

    constexpr double DB = 0.05;
    if (std::abs(errAz) < DB) errAz = 0.0;
    if (std::abs(errEl) < DB) errEl = 0.0;

    // ----------------------------------------------------
    // VELOCITY CONTROL (P + D on measurement)
    // ----------------------------------------------------
    double azVel = m_azPid.Kp * errAz;
    double elVel = m_elPid.Kp * errEl;
    if (dt > 0.0) {
        m_gimbalAzRate_dps = normalizeAngle180(data.gimbalAz - m_lastGimbalAz) / dt;
        m_gimbalElRate_dps = (data.gimbalEl - m_lastGimbalEl) / dt;
    }

    m_lastGimbalAz = data.gimbalAz;
    m_lastGimbalEl = data.gimbalEl;
    const double rateTau = 0.02; // 20 ms
    double alpha = dt / (rateTau + dt);
    m_gimbalAzRate_dps = alpha * m_gimbalAzRate_dps + (1.0 - alpha) * m_gimbalAzRate_dps;
    m_gimbalElRate_dps = alpha * m_gimbalElRate_dps + (1.0 - alpha) * m_gimbalElRate_dps;

    // Damping using gimbal rate (NOT error derivative)
    double azRate_dps = data.azRpm * 6.0;   // RPM → deg/s
    double elRate_dps = data.elRpm * 6.0;

    azVel += m_azPid.Kd * (m_smoothedAzVel_dps - azRate_dps);
    elVel += m_elPid.Kd * (m_smoothedElVel_dps - elRate_dps);

    if (std::abs(errAz) < DB && std::abs(azRate_dps) < 0.3) azVel = 0.0;
    if (std::abs(errEl) < DB && std::abs(elRate_dps) < 0.3) elVel = 0.0;
    // ----------------------------------------------------
    // TARGET MOTION FEED-FORWARD (ESSENTIAL)
    // ----------------------------------------------------
    azVel += m_smoothedAzVel_dps;
    elVel += m_smoothedElVel_dps;

    // ----------------------------------------------------
    // LAC (RATE BIAS, NEAR LOCK)
    // ----------------------------------------------------
    if (data.lacArmed &&
        data.leadAngleCompensationActive &&
        std::abs(errAz) < 1.0 &&
        std::abs(errEl) < 1.0) {

        azVel += LAC_RATE_BIAS_GAIN * data.lacLatchedAzRate_dps;
        elVel += LAC_RATE_BIAS_GAIN * data.lacLatchedElRate_dps;
    }

    // ----------------------------------------------------
    // VELOCITY LIMIT ONLY (NO ACCEL LIMIT)
    // ----------------------------------------------------
    constexpr double MAX_VEL = 10.0;
    azVel = qBound(-MAX_VEL, azVel, MAX_VEL);
    elVel = qBound(-MAX_VEL, elVel, MAX_VEL);


constexpr double STOP_ERR = 0.3;     // deg
constexpr double STOP_RATE = 0.5;    // deg/s

    if (std::abs(errAz) < STOP_ERR && std::abs(azRate_dps) > STOP_RATE)
        azVel = -0.6 * azRate_dps;   // active braking

    //Same for elevation.
    if (std::abs(errEl) < STOP_ERR && std::abs(elRate_dps) > STOP_RATE)
        elVel = -0.6 * elRate_dps;   // active braking
    

    azVel = -azVel;
    elVel = -elVel;
    sendStabilizedServoCommands(controller, azVel, elVel, false, dt);

    qDebug()
        << "errAz" << errAz
        << "azRpm" << data.azRpm
        << "azRate_dps" << azRate_dps
        << "targetRate" << m_smoothedAzVel_dps
        << "rateErr" << (m_smoothedAzVel_dps - azRate_dps)
        << "cmdAzVel" << azVel;
}

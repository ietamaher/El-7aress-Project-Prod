#include "trackingmotionmode.h"
#include "controllers/gimbalcontroller.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QDateTime>  // For throttling model updates
#include <QtGlobal>
#include <cmath>

// Define smoothing and rate limiting constants
static const double SMOOTHING_ALPHA = 0.3;           // Position smoothing (0.0 = no smoothing, 1.0 = no filtering)
static const double VELOCITY_SMOOTHING_ALPHA = 0.2;   // Velocity smoothing (more aggressive)
static const double MAX_VELOCITY = 15.0;              // Maximum velocity in deg/s
static const double MAX_ACCELERATION = 30.0;          // Maximum acceleration in deg/s²
static const double VELOCITY_CHANGE_LIMIT = 5.0;      // Maximum velocity change per update cycle

TrackingMotionMode::TrackingMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent), m_targetValid(false), m_targetAz(0.0), m_targetEl(0.0)
    , m_smoothedAzVel_dps(0.0), m_smoothedElVel_dps(0.0)
    , m_previousDesiredAzVel(0.0), m_previousDesiredElVel(0.0)  // Initialize previous velocities
{
    // Configure PID gains for STABLE and SMOOTH target tracking
    // Reduced gains to prevent motor overload
    m_azPid.Kp = 0.15;  // Reduced from 0.25
    m_azPid.Ki = 0.005; // Reduced from 0.01
    m_azPid.Kd = 0.01;  // Reduced from 0.02
    m_azPid.maxIntegral = 10.0; // Reduced from 20.0

    m_elPid.Kp = 0.15;  // Reduced from 0.25
    m_elPid.Ki = 0.005; // Reduced from 0.01
    m_elPid.Kd = 0.01;  // Reduced from 0.02
    m_elPid.maxIntegral = 10.0; // Reduced from 20.0
}

void TrackingMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[TrackingMotionMode] Enter";
    
    // Invalidate target on entering the mode to ensure we wait for a fresh command.
    m_targetValid = false;
    m_azPid.reset();
    m_elPid.reset();
    
    // Reset previous velocities for rate limiting
    m_previousDesiredAzVel = 0.0;
    m_previousDesiredElVel = 0.0;

    // ✅ CRITICAL: Start velocity timer for dt measurement
    startVelocityTimer();

    if (controller) {
        // Set MODERATE acceleration for responsive but smooth tracking
        // Reduced from 200000 to prevent motor overload
        if (auto azServo = controller->azimuthServo()) setAcceleration(azServo, 50000);
        if (auto elServo = controller->elevationServo()) setAcceleration(elServo, 50000);
    }
}

void TrackingMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[TrackingMotionMode] Exit";
    stopServos(controller);
}

void TrackingMotionMode::onTargetPositionUpdated(double az, double el, 
                                               double velocityAz_dps, double velocityEl_dps, 
                                               bool isValid)
{
    if (isValid) {
        if (!m_targetValid) {
             qDebug() << "[TrackingMotionMode] New valid target acquired.";
             m_azPid.reset();
             m_elPid.reset();
             m_smoothedTargetAz = az;
             m_smoothedTargetEl = el;
             // Initialize velocity smoother with first measured velocity
             m_smoothedAzVel_dps = velocityAz_dps;
             m_smoothedElVel_dps = velocityEl_dps;
        }
        m_targetValid = true;
        m_targetAz = az;
        m_targetEl = el;
        m_targetAzVel_dps = velocityAz_dps;
        m_targetElVel_dps = velocityEl_dps;

    } else {
        if (m_targetValid) {
             qDebug() << "[TrackingMotionMode] Target has been definitively lost.";
        }
        m_targetValid = false;
        // Reset velocities when target is lost
        m_targetAzVel_dps = 0.0;
        m_targetElVel_dps = 0.0;
        m_smoothedAzVel_dps = 0.0;
        m_smoothedElVel_dps = 0.0;
    }
}

// Helper function to apply rate limiting
double TrackingMotionMode::applyRateLimit(double newVelocity, double previousVelocity, double maxChange)
{
    double velocityChange = newVelocity - previousVelocity;
    if (std::abs(velocityChange) > maxChange) {
        // Limit the change to prevent sudden jumps
        return previousVelocity + (velocityChange > 0 ? maxChange : -maxChange);
    }
    return newVelocity;
}

double TrackingMotionMode::applyVelocityScaling(double velocity, double error) 
{
    // When error is zero, keep at 30% of full feed‑forward;
    // as error approaches threshold, ramp up to 100%.
    static constexpr double ERROR_THRESHOLD = 2.0;  // [deg]
    static constexpr double MIN_SCALE      = 0.3;   // 30%
    
    double absErr = std::abs(error);
    if (absErr >= ERROR_THRESHOLD) {
        // Outside threshold: no scaling
        return velocity;
    }
    
    // Compute normalized error [0,1]
    double norm = absErr / ERROR_THRESHOLD;
    // Quadratic blend: smoother ramp than linear
    double scale = MIN_SCALE + (1.0 - MIN_SCALE) * (norm * norm);
    
    return velocity * scale;
}

void TrackingMotionMode::update(GimbalController* controller)
{
    if (!m_targetValid) {
        stopServos(controller);
        return;
    }

    // ✅ CRITICAL FIX: Compute and clamp dt
    double dt_s = UPDATE_INTERVAL_S;
    if (m_velocityTimer.isValid()) {
        dt_s = clampDt(m_velocityTimer.restart() / 1000.0);
    } else {
        m_velocityTimer.start();
    }

    SystemStateData data = controller->systemStateModel()->data();

    // ✅ CRITICAL FIX: dt-aware smoothing using alphaFromTauDt (not fixed alpha!)
    const double tau_pos = 0.12; // 120ms time constant for position (tuneable)
    const double tau_vel = 0.08; // 80ms time constant for velocity (tuneable)
    double alphaPos = alphaFromTauDt(tau_pos, dt_s);
    double alphaVel = alphaFromTauDt(tau_vel, dt_s);

    // 1. Smooth the Target Position (for PID feedback)
    m_smoothedTargetAz = alphaPos * m_targetAz + (1.0 - alphaPos) * m_smoothedTargetAz;
    m_smoothedTargetEl = alphaPos * m_targetEl + (1.0 - alphaPos) * m_smoothedTargetEl;

    // 2. Smooth the Target Velocity (for feed-forward)
    m_smoothedAzVel_dps = alphaVel * m_targetAzVel_dps + (1.0 - alphaVel) * m_smoothedAzVel_dps;
    m_smoothedElVel_dps = alphaVel * m_targetElVel_dps + (1.0 - alphaVel) * m_smoothedElVel_dps;

    // ========================================================================
    // CRITICAL FIX: Apply Lead Angle to Gimbal Aim Point
    // ========================================================================
    // The tracker provides VISUAL target position (where target IS).
    // For moving targets, we must aim AHEAD (where target WILL BE).
    // Lead angle compensates for:
    //   - Bullet time-of-flight
    //   - Target motion during flight
    //   - Gravity drop
    //
    // This is the ONLY correct approach for mechanically coupled camera/gun:
    // The gimbal physically aims at the IMPACT POINT, not the visual target.
    // ========================================================================

    double aimPointAz = m_smoothedTargetAz;
    double aimPointEl = m_smoothedTargetEl;

    if (data.leadAngleCompensationActive && data.currentLeadAngleStatus == LeadAngleStatus::On) {
        // Add lead angle offsets to aim AHEAD of visual target
        aimPointAz += static_cast<double>(data.leadAngleOffsetAz);
        aimPointEl += static_cast<double>(data.leadAngleOffsetEl);

        /*qDebug() << "[TrackingMotionMode] AUTO-LEAD: Visual Target("
                 << m_smoothedTargetAz << "," << m_smoothedTargetEl
                 << ") + Lead(" << data.leadAngleOffsetAz << "," << data.leadAngleOffsetEl
                 << ") = Aim Point(" << aimPointAz << "," << aimPointEl << ")";*/
    }

    // 3. Calculate Position Error (using aim point, NOT visual target)
    double errAz = aimPointAz - data.gimbalAz;
    double errEl = aimPointEl - data.gimbalEl;  
    
    // Normalize azimuth error to [-180, 180] range
    while (errAz > 180.0) errAz -= 360.0;
    while (errAz < -180.0) errAz += 360.0;

    // 4. Calculate PID output (Feedback)
    bool useDerivativeOnMeasurement = true;
    // CRITICAL FIX: Use the measured dt_s, not the constant UPDATE_INTERVAL_S
    // CRITICAL FIX: Use aimPoint (with lead) for setpoint, NOT smoothedTarget (without lead)
    double pidAzVelocity = pidCompute(m_azPid, errAz, aimPointAz, data.gimbalAz, useDerivativeOnMeasurement, dt_s);
    double pidElVelocity = pidCompute(m_elPid, errEl, aimPointEl, data.gimbalEl, useDerivativeOnMeasurement, dt_s);

    // 5. Add Feed-forward term (scaled down to prevent aggressive response)
    const double FEEDFORWARD_GAIN = 0.5; // Scale down the feed-forward contribution
    double desiredAzVelocity = pidAzVelocity + (FEEDFORWARD_GAIN * m_smoothedAzVel_dps);
    double desiredElVelocity = pidElVelocity + (FEEDFORWARD_GAIN * m_smoothedElVel_dps);

    // 6. Apply velocity scaling based on error magnitude
    desiredAzVelocity = applyVelocityScaling(desiredAzVelocity, errAz);
    desiredElVelocity = applyVelocityScaling(desiredElVelocity, errEl);

    // 7. Apply system velocity constraints
    desiredAzVelocity = qBound(-MAX_VELOCITY, desiredAzVelocity, MAX_VELOCITY);
    desiredElVelocity = qBound(-MAX_VELOCITY, desiredElVelocity, MAX_VELOCITY);

    // ✅ CRITICAL FIX: Apply TIME-BASED rate limiting (not fixed per-cycle limit!)
    double maxDelta = MAX_ACCELERATION * dt_s; // deg/s^2 * s = deg/s
    desiredAzVelocity = applyRateLimitTimeBased(desiredAzVelocity, m_previousDesiredAzVel, maxDelta);
    desiredElVelocity = applyRateLimitTimeBased(desiredElVelocity, m_previousDesiredElVel, maxDelta);

    // 9. Store current velocities for next cycle
    m_previousDesiredAzVel = desiredAzVelocity;
    m_previousDesiredElVel = desiredElVelocity;

    // ✅ CRITICAL FIX: Throttle model updates to 10 Hz (Expert Review)
    // 10. Convert target to world-frame for AHRS-based stabilization
    static qint64 lastPublishMs = 0;
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - lastPublishMs >= 100) { // 10 Hz throttle
        if (data.imuConnected) {
            // CRITICAL FIX: Convert AIM POINT (with lead) to world frame, NOT visual target
            // This ensures stabilization maintains the ballistic solution, not just visual lock
            double worldAz, worldEl;
            convertGimbalToWorldFrame(aimPointAz, aimPointEl,
                                      data.imuRollDeg, data.imuPitchDeg, data.imuYawDeg,
                                      worldAz, worldEl);

            // Update system state with world-frame target
            auto stateModel = controller->systemStateModel();
            SystemStateData updatedState = stateModel->data();
            updatedState.targetAzimuth_world = worldAz;
            updatedState.targetElevation_world = worldEl;
            updatedState.useWorldFrameTarget = true; // Enable world-frame tracking
            stateModel->updateData(updatedState);
        }
        lastPublishMs = nowMs;
    }

    // Debug output (reduced frequency to avoid spam)
    /*static int debugCounter = 0;
    if (++debugCounter % 10 == 0) { // Print every 10 updates
        qDebug() << "Tracking - Error(Az,El):" << errAz << "," << errEl
                 << "| Vel(Az,El):" << desiredAzVelocity << "," << desiredElVelocity
                 << "| FF(Az,El):" << m_smoothedAzVel_dps << "," << m_smoothedElVel_dps
                 << "| elapsed(s):" << dt_s;
    }*/

    // 11. Send final commands with stabilization enabled
    // Hybrid stabilization adds platform motion compensation to tracking velocity
    sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity, true);
}


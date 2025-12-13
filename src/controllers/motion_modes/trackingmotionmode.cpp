#include "trackingmotionmode.h"
#include "controllers/gimbalcontroller.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QDateTime>  // For throttling model updates
#include <QtGlobal>
#include <cmath>

TrackingMotionMode::TrackingMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent), m_targetValid(false), m_targetAz(0.0), m_targetEl(0.0)
    , m_smoothedAzVel_dps(0.0), m_smoothedElVel_dps(0.0)
    , m_previousDesiredAzVel(0.0), m_previousDesiredElVel(0.0)
{
    // ✅ Load PID gains from runtime config (field-tunable without rebuild)
    const auto& cfg = MotionTuningConfig::instance();

    m_azPid.Kp = cfg.trackingAz.kp;
    m_azPid.Ki = cfg.trackingAz.ki;
    m_azPid.Kd = cfg.trackingAz.kd;
    m_azPid.maxIntegral = cfg.trackingAz.maxIntegral;

    m_elPid.Kp = cfg.trackingEl.kp;
    m_elPid.Ki = cfg.trackingEl.ki;
    m_elPid.Kd = cfg.trackingEl.kd;
    m_elPid.maxIntegral = cfg.trackingEl.maxIntegral;
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

void TrackingMotionMode::update(GimbalController* controller, double dt)
{
    if (!m_targetValid) {
        stopServos(controller);
        return;
    }

    SystemStateData data = controller->systemStateModel()->data();
    const auto& cfg = MotionTuningConfig::instance();

    // --- 1. Smooth target position and velocity ---
    double alphaPos = alphaFromTauDt(cfg.filters.trackingPositionTau, dt);
    double alphaVel = alphaFromTauDt(cfg.filters.trackingVelocityTau, dt);

    m_smoothedTargetAz = alphaPos * m_targetAz + (1.0 - alphaPos) * m_smoothedTargetAz;
    m_smoothedTargetEl = alphaPos * m_targetEl + (1.0 - alphaPos) * m_smoothedTargetEl;

    m_smoothedAzVel_dps = alphaVel * m_targetAzVel_dps + (1.0 - alphaVel) * m_smoothedAzVel_dps;
    m_smoothedElVel_dps = alphaVel * m_targetElVel_dps + (1.0 - alphaVel) * m_smoothedElVel_dps;

    // --- 2. Compute VISUAL error (target position without corrections) ---
    // This tells us if the visual target is centered in the reticle
    double visualErrAz = normalizeAngle180(m_smoothedTargetAz - data.gimbalAz);
    double visualErrEl = m_smoothedTargetEl - data.gimbalEl;
    double visualErrorMag = std::sqrt(visualErrAz * visualErrAz + visualErrEl * visualErrEl);

    // --- 3. Compute AIM POINT (with ballistics + lead) ---
    double aimAz = m_smoothedTargetAz;
    double aimEl = m_smoothedTargetEl;

    bool correctionsActive = false;

    if (data.ballisticDropActive) {
        aimAz += data.ballisticDropOffsetAz;
        aimEl -= data.ballisticDropOffsetEl;
        correctionsActive = true;
    }

    if (data.leadAngleCompensationActive &&
        (data.currentLeadAngleStatus == LeadAngleStatus::On ||
         data.currentLeadAngleStatus == LeadAngleStatus::Lag))
    {
        aimAz += data.motionLeadOffsetAz;
        aimEl += data.motionLeadOffsetEl;
        correctionsActive = true;
    }

    // --- 4. Compute AIM error (what gimbal must track) ---
    double errAz = normalizeAngle180(aimAz - data.gimbalAz);
    double errEl = aimEl - data.gimbalEl;

    // --- 5. DEADBAND based on aim error ---
    static constexpr double DEADBAND = 0.3;
    
    bool azInDeadband = std::abs(errAz) < DEADBAND;
    bool elInDeadband = std::abs(errEl) < DEADBAND;
    
    if (azInDeadband) {
        errAz = 0.0;
        m_azPid.integral *= 0.9;
    }
    if (elInDeadband) {
        errEl = 0.0;
        m_elPid.integral *= 0.9;
    }

    if (azInDeadband && elInDeadband) {
        m_previousDesiredAzVel *= 0.8;
        m_previousDesiredElVel *= 0.8;
        m_azPid.integral = 0.0;
        m_elPid.integral = 0.0;
        sendStabilizedServoCommands(controller, m_previousDesiredAzVel, m_previousDesiredElVel, false, dt);
        return;
    }

    // --- 6. PID FEEDBACK (on aim error) ---
    bool derivativeOnMeasurement = true;
    double pidAzVelocity = pidCompute(m_azPid, errAz, aimAz, data.gimbalAz, derivativeOnMeasurement, dt);
    double pidElVelocity = pidCompute(m_elPid, errEl, aimEl, data.gimbalEl, derivativeOnMeasurement, dt);

    // --- 7. FEEDFORWARD - Different logic for LAC vs no-LAC ---
    double ffAz = 0.0;
    double ffEl = 0.0;

    if (correctionsActive) {
        // ================================================================
        // LAC/DROP ACTIVE: Use VISUAL error to determine FF
        // ================================================================
        // When corrections are active, the aim point is offset from visual target.
        // We should apply FF based on whether the VISUAL target is centered,
        // not the aim point.
        //
        // If visual target is centered → target motion is being tracked → apply FF
        // If visual target is offset → still catching up → reduce FF
        
        double FF = 0.0;
        if (visualErrorMag < 1.5) {
            // Visual target nearly centered - we're tracking well
            // Apply FF to help follow target motion
            FF = 0.7 * std::max(0.0, 1.0 - visualErrorMag / 1.5);
        }

        ffAz = FF * m_smoothedAzVel_dps;
        ffEl = FF * m_smoothedElVel_dps;

        // Debug for LAC/drop mode
        static int lacDbg = 0;
        if (++lacDbg % 20 == 0) {
            qDebug() << "[TRACKING+FCS] visualErr=" << QString::number(visualErrorMag, 'f', 2)
                     << "aimErr=(" << QString::number(errAz, 'f', 2) << "," << QString::number(errEl, 'f', 2) << ")"
                     << "FF=" << QString::number(FF, 'f', 2)
                     << "drop=(" << data.ballisticDropOffsetAz << "," << data.ballisticDropOffsetEl << ")"
                     << "lead=(" << data.motionLeadOffsetAz << "," << data.motionLeadOffsetEl << ")";
        }
    }
    else {
        // ================================================================
        // NO CORRECTIONS: Use aim error (same as visual error)
        // ================================================================
        double aimErrorMag = std::sqrt(errAz * errAz + errEl * errEl);
        double FF = 0.0;
        
        if (aimErrorMag < 1.0) {
            FF = 0.5 * (1.0 - aimErrorMag);
        }

        ffAz = FF * m_smoothedAzVel_dps;
        ffEl = FF * m_smoothedElVel_dps;
    }

    double desiredAzVelocity = pidAzVelocity + ffAz;
    double desiredElVelocity = pidElVelocity + ffEl;

    // --- 8. Velocity limits ---
    static constexpr double TRACKING_MAX_VEL = 15.0;
    desiredAzVelocity = qBound(-TRACKING_MAX_VEL, desiredAzVelocity, TRACKING_MAX_VEL);
    desiredElVelocity = qBound(-TRACKING_MAX_VEL, desiredElVelocity, TRACKING_MAX_VEL);

    // --- 9. Acceleration rate limiting ---
    double maxDelta = cfg.motion.maxAccelerationDegS2 * dt;
    desiredAzVelocity = applyRateLimitTimeBased(desiredAzVelocity, m_previousDesiredAzVel, maxDelta);
    desiredElVelocity = applyRateLimitTimeBased(desiredElVelocity, m_previousDesiredElVel, maxDelta);

    // --- DEBUG ---
    static int diagCnt = 0;
    if (++diagCnt % 10 == 0) {
        qDebug() << "[TRACKING] err=(" << QString::number(errAz, 'f', 2) 
                 << "," << QString::number(errEl, 'f', 2) << ")"
                 << "pid=(" << QString::number(pidAzVelocity, 'f', 2)
                 << "," << QString::number(pidElVelocity, 'f', 2) << ")"
                 << "ff=(" << QString::number(ffAz, 'f', 2)
                 << "," << QString::number(ffEl, 'f', 2) << ")"
                 << "cmd=(" << QString::number(desiredAzVelocity, 'f', 2)
                 << "," << QString::number(desiredElVelocity, 'f', 2) << ")"
                 << (correctionsActive ? "[FCS]" : "");
    }

    m_previousDesiredAzVel = desiredAzVelocity;
    m_previousDesiredElVel = desiredElVelocity;

    // --- 10. World-frame target for stabilization (throttled) ---
    static qint64 lastPubMs = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastPubMs >= 100 && data.imuConnected) {
        double worldAz, worldEl;
        convertGimbalToWorldFrame(aimAz, aimEl,
                                  data.imuRollDeg, data.imuPitchDeg, data.imuYawDeg,
                                  worldAz, worldEl);
        auto model = controller->systemStateModel();
        auto st = model->data();
        st.targetAzimuth_world = worldAz;
        st.targetElevation_world = worldEl;
        st.useWorldFrameTarget = true;
        model->updateData(st);
        lastPubMs = now;
    }

    // --- 11. Send command ---
    sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity, false, dt);
}
 

/*
void TrackingMotionMode::update(GimbalController* controller, double dt)
{
    if (!m_targetValid) {
        stopServos(controller);
        return;
    }

    // ✅ EXPERT REVIEW FIX: dt is now passed from GimbalController (centralized measurement)

    SystemStateData data = controller->systemStateModel()->data();

    // ✅ EXPERT REVIEW FIX: dt-aware smoothing using runtime-configurable tau
    const auto& cfg = MotionTuningConfig::instance();
    double alphaPos = alphaFromTauDt(cfg.filters.trackingPositionTau, dt);
    double alphaVel = alphaFromTauDt(cfg.filters.trackingVelocityTau, dt);

    // 1. Smooth the Target Position (for PID feedback)
    m_smoothedTargetAz = alphaPos * m_targetAz + (1.0 - alphaPos) * m_smoothedTargetAz;
    m_smoothedTargetEl = alphaPos * m_targetEl + (1.0 - alphaPos) * m_smoothedTargetEl;

    // 2. Smooth the Target Velocity (for feed-forward)
    m_smoothedAzVel_dps = alphaVel * m_targetAzVel_dps + (1.0 - alphaVel) * m_smoothedAzVel_dps;
    m_smoothedElVel_dps = alphaVel * m_targetElVel_dps + (1.0 - alphaVel) * m_smoothedElVel_dps;

    // ========================================================================
    // PROFESSIONAL FCS: Apply Ballistic Corrections to Gimbal Aim Point
    // ========================================================================
    // The tracker provides VISUAL target position (where target IS).
    // For accurate fire control, we apply corrections in order:
    //   1. BALLISTIC DROP (auto when range valid) - gravity + wind
    //   2. MOTION LEAD (when LAC active) - moving target compensation
    //
    // This is the professional approach (Kongsberg/Rafael standard):
    // The gimbal physically aims at the IMPACT POINT, not the visual target.
    // ========================================================================

    double aimPointAz = m_smoothedTargetAz;
    double aimPointEl = m_smoothedTargetEl;

    // STEP 1: Apply ballistic drop (auto when LRF range valid)
    if (data.ballisticDropActive) {
        aimPointAz += static_cast<double>(data.ballisticDropOffsetAz);
        aimPointEl += static_cast<double>(data.ballisticDropOffsetEl);
    }

    // STEP 2: Apply motion lead (only when LAC toggle active)
    // BUG FIX #2: Include Lag status (clamped lead is still applied)
    if (data.leadAngleCompensationActive &&
        (data.currentLeadAngleStatus == LeadAngleStatus::On ||
         data.currentLeadAngleStatus == LeadAngleStatus::Lag)) {
        aimPointAz += static_cast<double>(data.motionLeadOffsetAz);
        aimPointEl += static_cast<double>(data.motionLeadOffsetEl);

        //qDebug() << "[TrackingMotionMode] FULL FCS: Visual Target("
        //         << m_smoothedTargetAz << "," << m_smoothedTargetEl
       //         << ") + Drop(" << data.ballisticDropOffsetAz << "," << data.ballisticDropOffsetEl
        //         << ") + Lead(" << data.motionLeadOffsetAz << "," << data.motionLeadOffsetEl
         //        << ") = Aim Point(" << aimPointAz << "," << aimPointEl << ")";
    }

    // 3. Calculate Position Error (using aim point, NOT visual target)
    double errAz = aimPointAz - data.gimbalAz;
    double errEl = aimPointEl - data.gimbalEl;

    // Normalize azimuth error to [-180, 180] range
    errAz = normalizeAngle180(errAz);

    // 4. Calculate PID output (Feedback)
    bool useDerivativeOnMeasurement = true;
    // CRITICAL FIX: Use the measured dt, not the constant UPDATE_INTERVAL_S
    // CRITICAL FIX: Use aimPoint (with lead) for setpoint, NOT smoothedTarget (without lead)
    double pidAzVelocity = pidCompute(m_azPid, errAz, aimPointAz, data.gimbalAz, useDerivativeOnMeasurement, dt);
    double pidElVelocity = pidCompute(m_elPid, errEl, aimPointEl, data.gimbalEl, useDerivativeOnMeasurement, dt);

    // 5. Add Feed-forward term (scaled down to prevent aggressive response)
    const double FEEDFORWARD_GAIN = 0.5; // Scale down the feed-forward contribution
    double desiredAzVelocity = pidAzVelocity + (FEEDFORWARD_GAIN * m_smoothedAzVel_dps);
    double desiredElVelocity = pidElVelocity + (FEEDFORWARD_GAIN * m_smoothedElVel_dps);

    // 6. Apply velocity scaling based on error magnitude
    desiredAzVelocity = applyVelocityScaling(desiredAzVelocity, errAz);
    desiredElVelocity = applyVelocityScaling(desiredElVelocity, errEl);

    // 7. Apply system velocity constraints (from config)
    double maxVel = cfg.motion.maxVelocityDegS;
    desiredAzVelocity = qBound(-maxVel, desiredAzVelocity, maxVel);
    desiredElVelocity = qBound(-maxVel, desiredElVelocity, maxVel);

    // ✅ CRITICAL FIX: Apply TIME-BASED rate limiting (from config)
    double maxDelta = cfg.motion.maxAccelerationDegS2 * dt; // deg/s^2 * s = deg/s
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
    //static int debugCounter = 0;
   // if (++debugCounter % 10 == 0) { // Print every 10 updates
    //    qDebug() << "Tracking - Error(Az,El):" << errAz << "," << errEl
   //              << "| Vel(Az,El):" << desiredAzVelocity << "," << desiredElVelocity
    //             << "| FF(Az,El):" << m_smoothedAzVel_dps << "," << m_smoothedElVel_dps
    //             << "| elapsed(s):" << dt;
   // } 

    // 11. Send final commands with stabilization enabled
    // Hybrid stabilization adds platform motion compensation to tracking velocity
    sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity, true, dt);
}
*/

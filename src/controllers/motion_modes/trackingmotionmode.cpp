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
    // ==========================================================================
    // CROWS-COMPLIANT TRACKING (TM 9-1090-225-10-2)
    // ==========================================================================
    // KEY DOCTRINE CHANGES:
    // 1. PID uses VISUAL target only - NO ballistics in gimbal control
    // 2. Palm switch must be HELD for active tracking
    // 3. Dead reckoning during firing (hold last velocity)
    // 4. LAC is rate-bias feed-forward, NOT position steering
    // 5. Manual override allowed within half-screen
    //
    // Ballistic corrections (drop + lead) affect CCIP display ONLY,
    // not gimbal control. The operator fires when target overlaps CCIP.
    // ==========================================================================

    SystemStateData data = controller->systemStateModel()->data();
    const auto& cfg = MotionTuningConfig::instance();

    // ==========================================================================
    // PHASE 1: DEAD RECKONING CHECK
    // ==========================================================================
    // "When firing is initiated, CROWS aborts Target Tracking. Instead the system
    //  moves according to the speed and direction of the WS just prior to pulling
    //  the trigger." - TM 9-1090-225-10-2 page 38
    if (data.deadReckoningActive) {
        // Maintain constant velocity from moment of firing
        sendStabilizedServoCommands(
            controller,
            data.deadReckoningAzVel_dps,
            data.deadReckoningElVel_dps,
            true, dt
        );
        return;
    }

    // ==========================================================================
    // PHASE 2: PALM SWITCH CHECK
    // ==========================================================================
    // "Target Tracking actively tracks a target only when the Palm Switch is held.
    //  To stop active tracking at any time, release the Palm Switch."
    // - TM 9-1090-225-10-2 page 39
    if (!data.deadManSwitchActive) {
        // Palm switch released - pause tracking (coast mode)
        // Gradually decelerate to stop
        m_previousDesiredAzVel *= 0.9;
        m_previousDesiredElVel *= 0.9;

        if (std::abs(m_previousDesiredAzVel) < 0.1 && std::abs(m_previousDesiredElVel) < 0.1) {
            stopServos(controller);
        } else {
            sendStabilizedServoCommands(controller, m_previousDesiredAzVel, m_previousDesiredElVel, true, dt);
        }
        return;
    }

    // ==========================================================================
    // PHASE 3: TARGET VALIDITY CHECK
    // ==========================================================================
    if (!m_targetValid) {
        stopServos(controller);
        return;
    }

    // ==========================================================================
    // PHASE 4: SMOOTH TARGET POSITION AND VELOCITY
    // ==========================================================================
    double alphaPos = alphaFromTauDt(cfg.filters.trackingPositionTau, dt);
    double alphaVel = alphaFromTauDt(cfg.filters.trackingVelocityTau, dt);

    m_smoothedTargetAz = alphaPos * m_targetAz + (1.0 - alphaPos) * m_smoothedTargetAz;
    m_smoothedTargetEl = alphaPos * m_targetEl + (1.0 - alphaPos) * m_smoothedTargetEl;

    m_smoothedAzVel_dps = alphaVel * m_targetAzVel_dps + (1.0 - alphaVel) * m_smoothedAzVel_dps;
    m_smoothedElVel_dps = alphaVel * m_targetElVel_dps + (1.0 - alphaVel) * m_smoothedElVel_dps;

    // ==========================================================================
    // PHASE 5: COMPUTE VISUAL ERROR (CROWS-COMPLIANT)
    // ==========================================================================
    // CRITICAL: PID uses VISUAL target position ONLY
    // NO ballistic drop, NO motion lead in gimbal control!
    // The gimbal tracks WHERE THE TARGET IS, not where the bullet will land.
    double errAz = -(normalizeAngle180(m_smoothedTargetAz - data.gimbalAz));
    double errEl = -(m_smoothedTargetEl - data.gimbalEl);

    // ==========================================================================
    // PHASE 6: MANUAL OVERRIDE DURING TRACKING
    // ==========================================================================
    // "CROWS maintains Target Tracking while the reticle is moved to another
    //  point (no more than half a screen from tracked target)."
    // - TM 9-1090-225-10-2 page 39
    double manualAzVel = 0.0;
    double manualElVel = 0.0;

    const float JOYSTICK_DEADBAND = 0.15f;
    bool hasManualInput = (std::abs(data.joystickAzValue) > JOYSTICK_DEADBAND ||
                           std::abs(data.joystickElValue) > JOYSTICK_DEADBAND);

    if (hasManualInput) {
        // Apply manual velocity on top of tracking
        // Scale factor converts joystick (-1 to 1) to deg/s
        manualAzVel = data.joystickAzValue * MANUAL_OVERRIDE_SCALE;
        manualElVel = data.joystickElValue * MANUAL_OVERRIDE_SCALE; // Inverted Y

        // Note: Manual input should NOT silently re-arm LAC
        // This is handled by the LAC latching system - manual input is independent

        static int manualDbg = 0;
        if (++manualDbg % 20 == 0) {
            qDebug() << "[CROWS] Manual override during tracking: Az=" << manualAzVel
                     << "°/s El=" << manualElVel << "°/s";
        }
    }

    // ==========================================================================
    // PHASE 7: DEADBAND
    // ==========================================================================
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

    // If both in deadband and no manual input, hold position
    if (azInDeadband && elInDeadband && !hasManualInput) {
        m_previousDesiredAzVel *= 0.8;
        m_previousDesiredElVel *= 0.8;
        m_azPid.integral = 0.0;
        m_elPid.integral = 0.0;
        sendStabilizedServoCommands(controller, m_previousDesiredAzVel, m_previousDesiredElVel, true, dt);
        return;
    }

    // ==========================================================================
    // PHASE 8: PID FEEDBACK (on VISUAL error only!)
    // ==========================================================================
    bool derivativeOnMeasurement = true;
    double pidAzVelocity = pidCompute(m_azPid, errAz, m_smoothedTargetAz, data.gimbalAz, derivativeOnMeasurement, dt);
    double pidElVelocity = pidCompute(m_elPid, errEl, m_smoothedTargetEl, data.gimbalEl, derivativeOnMeasurement, dt);

    // ==========================================================================
    // PHASE 9: LAC RATE-BIAS FEED-FORWARD (CROWS-COMPLIANT)
    // ==========================================================================
    // "Lead Angle Compensation helps to keep the on screen reticle on target while
    //  the Host Platform, the target, or both are moving at a near constant speed"
    //
    // CROWS LAC is a RATE BIAS - it adds to velocity command, NOT position error.
    // This helps the operator maintain tracking without constant joystick input.
    // The reticle stays ON the target, while CCIP shows predicted impact.
    double lacAzBias = 0.0;
    double lacElBias = 0.0;

    if (data.lacArmed && data.leadAngleCompensationActive) {
        // Use LATCHED rates for rate bias (not current rates)
        // This prevents auto-adaptation when target changes
        lacAzBias = LAC_RATE_BIAS_GAIN * data.lacLatchedAzRate_dps;
        lacElBias = LAC_RATE_BIAS_GAIN * data.lacLatchedElRate_dps;

        static int lacDbg = 0;
        if (++lacDbg % 40 == 0) {
            qDebug() << "[CROWS LAC] Rate bias: Az=" << lacAzBias << "°/s El=" << lacElBias << "°/s"
                     << "| Latched rates: Az=" << data.lacLatchedAzRate_dps
                     << "El=" << data.lacLatchedElRate_dps;
        }
    }

    // ==========================================================================
    // PHASE 10: BASIC FEED-FORWARD (target velocity assistance)
    // ==========================================================================
    // This is separate from LAC - basic FF helps smooth tracking regardless
    double visualErrorMag = std::sqrt(errAz * errAz + errEl * errEl);
    double ffGain = 0.0;

    if (visualErrorMag < 1.0) {
        // Target nearly centered - apply feed-forward
        ffGain = 0.5 * (1.0 - visualErrorMag);
    }

    double ffAz = ffGain * m_smoothedAzVel_dps;
    double ffEl = ffGain * m_smoothedElVel_dps;

    // ==========================================================================
    // PHASE 11: COMBINE ALL VELOCITY COMPONENTS
    // ==========================================================================
    // Final velocity = PID + FF + LAC rate bias + Manual override
    double desiredAzVelocity = pidAzVelocity + ffAz + lacAzBias + manualAzVel;
    double desiredElVelocity = pidElVelocity + ffEl + lacElBias + manualElVel;

    // ==========================================================================
    // PHASE 12: VELOCITY LIMITS
    // ==========================================================================
    static constexpr double TRACKING_MAX_VEL = 15.0;
    desiredAzVelocity = qBound(-TRACKING_MAX_VEL, desiredAzVelocity, TRACKING_MAX_VEL);
    desiredElVelocity = qBound(-TRACKING_MAX_VEL, desiredElVelocity, TRACKING_MAX_VEL);

    // ==========================================================================
    // PHASE 13: ACCELERATION RATE LIMITING
    // ==========================================================================
    double maxDelta = cfg.motion.maxAccelerationDegS2 * dt;
    desiredAzVelocity = applyRateLimitTimeBased(desiredAzVelocity, m_previousDesiredAzVel, maxDelta);
    desiredElVelocity = applyRateLimitTimeBased(desiredElVelocity, m_previousDesiredElVel, maxDelta);

    // ==========================================================================
    // PHASE 14: DEBUG OUTPUT
    // ==========================================================================
    static int diagCnt = 0;
    if (++diagCnt % 10 == 0) {
        qDebug() << "[CROWS TRACKING] err=(" << QString::number(errAz, 'f', 2)
                 << "," << QString::number(errEl, 'f', 2) << ")"
                 << "pid=(" << QString::number(pidAzVelocity, 'f', 2)
                 << "," << QString::number(pidElVelocity, 'f', 2) << ")"
                 << "ff=(" << QString::number(ffAz, 'f', 2)
                 << "," << QString::number(ffEl, 'f', 2) << ")"
                 << "lac=(" << QString::number(lacAzBias, 'f', 2)
                 << "," << QString::number(lacElBias, 'f', 2) << ")"
                 << "cmd=(" << QString::number(desiredAzVelocity, 'f', 2)
                 << "," << QString::number(desiredElVelocity, 'f', 2) << ")"
                 << (data.lacArmed ? "[LAC]" : "");
    }

    m_previousDesiredAzVel = desiredAzVelocity;
    m_previousDesiredElVel = desiredElVelocity;

    // ==========================================================================
    // PHASE 15: WORLD-FRAME TARGET FOR STABILIZATION (throttled)
    // ==========================================================================
    // Note: For stabilization, we use VISUAL target (not aim point with ballistics)
    // This maintains visual lock during platform motion
    static qint64 lastPubMs = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastPubMs >= 100 && data.imuConnected) {
        double worldAz, worldEl;
        convertGimbalToWorldFrame(m_smoothedTargetAz, m_smoothedTargetEl,
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

    // ==========================================================================
    // PHASE 16: SEND SERVO COMMANDS
    // ==========================================================================
    sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity, true, dt);
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

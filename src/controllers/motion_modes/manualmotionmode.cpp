#include "manualmotionmode.h"
#include "controllers/gimbalcontroller.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QDateTime>  // For throttling model updates

ManualMotionMode::ManualMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent)
{
}

void ManualMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[ManualMotionMode] Enter";

    if (!controller) return;
    m_currentAzVelocityCmd = 0.0;
    m_currentElVelocityCmd = 0.0;

    m_filteredAzJoystick = 0.0;
    m_filteredElJoystick = 0.0;

    // ✅ CRITICAL: Start velocity timer for dt measurement
    startVelocityTimer();

    // Set initial acceleration for both servos
    if (auto azServo = controller->azimuthServo()) {
        setAcceleration(azServo);
    }
    if (auto elServo = controller->elevationServo()) {
        setAcceleration(elServo);
    }
}

void ManualMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[ManualMotionMode] Exit";
    stopServos(controller);
}

void ManualMotionMode::update(GimbalController* controller, double dt)
{
    if (!checkSafetyConditions(controller)) {
        stopServos(controller);
        return;
    }

    SystemStateData data = controller->systemStateModel()->data();

    // ✅ EXPERT REVIEW FIX: dt is now passed from GimbalController (centralized measurement)

    // 1. Calculate TARGET velocity in the motor's native units
    static constexpr double MAX_SPEED_HZ = 35000.0;
    double speedPercent = data.gimbalSpeed / 100.0;
    double maxCurrentSpeedHz = speedPercent * MAX_SPEED_HZ;

    // ✅ EXPERT REVIEW FIX: dt-aware joystick filter using runtime-configurable tau
    const auto& cfg = MotionTuningConfig::instance();
    double alpha = alphaFromTauDt(cfg.filters.manualJoystickTau, dt);

    // 2. Get and filter joystick values
    double rawAzJoystick = data.joystickAzValue;
    double rawElJoystick = data.joystickElValue;

    m_filteredAzJoystick = alpha * rawAzJoystick + (1.0 - alpha) * m_filteredAzJoystick;
    m_filteredElJoystick = alpha * rawElJoystick + (1.0 - alpha) * m_filteredElJoystick;

    // ✅ EXPERT REVIEW FIX: Apply shaping ONLY (no double-filtering!)
    // Filter already applied above, now only shape the response curve
    double shaped_stick_Azinput = processJoystickInput(m_filteredAzJoystick);
    double shaped_stick_Elinput = processJoystickInput(m_filteredElJoystick);

    // 3. Calculate target speeds
    double targetAzSpeedHz = shaped_stick_Azinput * maxCurrentSpeedHz;
    double targetElSpeedHz = shaped_stick_Elinput * maxCurrentSpeedHz;

    // 4. Define control parameters
    static constexpr double DEADBAND_HZ = 100.0;

    // Apply deadband to the target speed
    if (std::abs(targetAzSpeedHz) < DEADBAND_HZ) targetAzSpeedHz = 0.0;
    if (std::abs(targetElSpeedHz) < DEADBAND_HZ) targetElSpeedHz = 0.0;

    // ✅ EXPERT REVIEW FIX: Time-based rate limiter in Hz domain (from config)
    double maxChangeHz = cfg.manualLimits.maxAccelHzPerSec * dt; // Hz/s * s = Hz

    // ✅ Apply time-based rate limiting using central helper
    m_currentAzSpeedCmd_Hz = applyRateLimitTimeBased(targetAzSpeedHz, m_currentAzSpeedCmd_Hz, maxChangeHz);
    m_currentElSpeedCmd_Hz = applyRateLimitTimeBased(targetElSpeedHz, m_currentElSpeedCmd_Hz, maxChangeHz);

    // ✅ Use centralized unit conversions (no more magic numbers!)
    double azVelocityDegS = m_currentAzSpeedCmd_Hz / AZ_STEPS_PER_DEGREE();
    double elVelocityDegS = m_currentElSpeedCmd_Hz / EL_STEPS_PER_DEGREE();

    // 5. World-frame target management
    constexpr double VELOCITY_THRESHOLD = 0.1; // deg/s
    bool joystickActive = (std::abs(azVelocityDegS) > VELOCITY_THRESHOLD ||
                           std::abs(elVelocityDegS) > VELOCITY_THRESHOLD);

    // ✅ Throttle model updates to 10 Hz (Expert Review)
    static qint64 lastPublishMs = 0;
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (joystickActive) {
        // Joystick is moving - disable world-frame hold, update target periodically
        if (nowMs - lastPublishMs >= 100 && data.imuConnected) {
            // Update world-frame target to current pointing direction
            // This way when joystick is released, gimbal holds current direction
            double worldAz, worldEl;
            convertGimbalToWorldFrame(data.gimbalAz, data.gimbalEl,
                                      data.imuRollDeg, data.imuPitchDeg, data.imuYawDeg,
                                      worldAz, worldEl);

            // Update system state model with new world target
            auto stateModel = controller->systemStateModel();
            SystemStateData updatedState = stateModel->data();
            updatedState.targetAzimuth_world = worldAz;
            updatedState.targetElevation_world = worldEl;
            updatedState.useWorldFrameTarget = false; // Disable hold while moving
            stateModel->updateData(updatedState);
            lastPublishMs = nowMs;
        }
    } else {
        // Joystick centered - enable world-frame hold (state change, immediate update)
        if (data.imuConnected) {
            auto stateModel = controller->systemStateModel();
            SystemStateData updatedState = stateModel->data();
            updatedState.useWorldFrameTarget = true; // Enable world-frame stabilization
            stateModel->updateData(updatedState);
        }
    }

    // 6. Send final command (stabilization handles world-frame hold)
    sendStabilizedServoCommands(controller, azVelocityDegS, elVelocityDegS, true, dt);
}

// ✅ EXPERT REVIEW FIX: Removed double-filtering - this now only shapes the response curve
double ManualMotionMode::processJoystickInput(double filteredInput) {
    const double exponent = 1.5;

    // Apply power curve for finer control near center
    double shaped = std::pow(std::abs(filteredInput), exponent);
    return (filteredInput < 0) ? -shaped : shaped;
}





/*
    // The top-level safety check is now performed by GimbalController::update()
    // before this method is called. We can proceed directly to the mode's logic.

    SystemStateData data = controller->systemStateModel()->data();
    
    float angularVelocity = data.gimbalSpeed * SPEED_MULTIPLIER;
    
    double azInput = data.joystickAzValue;
    double elInput = data.joystickElValue;

    if (!checkElevationLimits(data.gimbalEl, elInput, 
                             data.upperLimitSensorActive, 
                             data.lowerLimitSensorActive)) {
        elInput = 0.0;
        qDebug() << "[ManualMotionMode] Elevation limit reached";
    }

     // The SPEED_MULTIPLIER now directly converts the speed setting to max deg/s
    // Example: If gimbalSpeed is 1-10, multiplier could be 3.0 to get a max of 30 deg/s
    static constexpr float SPEED_MULTIPLIER = 1.0f;
    float maxSpeedDegS = data.gimbalSpeed * SPEED_MULTIPLIER;
    double targetAzVelocity = data.joystickAzValue * maxSpeedDegS;
    double targetElVelocity = data.joystickElValue * maxSpeedDegS;
    
    // 2. Apply a proper, state-aware Rate Limiter.
    double maxVelocityChange = MAX_MANUAL_ACCEL_DEGS2 * UPDATE_INTERVAL_S;

    // --- Azimuth Axis ---
    if (std::abs(targetAzVelocity) > std::abs(m_currentAzVelocityCmd)) {
        // CASE 1: ACCELERATING or REVERSING
        // We are commanding a higher speed than the current one.
        // Ramp up smoothly to prevent current spikes.
        double error = targetAzVelocity - m_currentAzVelocityCmd;
        m_currentAzVelocityCmd += qBound(-maxVelocityChange, error, maxVelocityChange);
    } else {
        // CASE 2: DECELERATING or STOPPING
        // We are commanding a lower speed, or zero.
        // In this case, we command the target speed *directly*.
        // This allows the motor driver's own deceleration ramp to take over,
        // resulting in a crisp, predictable stop.
        m_currentAzVelocityCmd = targetAzVelocity;
    }

    // --- Elevation Axis (identical logic) ---
    if (std::abs(targetElVelocity) > std::abs(m_currentElVelocityCmd)) {
        // CASE 1: ACCELERATING or REVERSING
        double error = targetElVelocity - m_currentElVelocityCmd;
        m_currentElVelocityCmd += qBound(-maxVelocityChange, error, maxVelocityChange);
    } else {
        // CASE 2: DECELERATING or STOPPING
        m_currentElVelocityCmd = targetElVelocity;
    }

    // 3. Send the properly-ramped command to the driver.
    sendStabilizedServoCommands(controller, int(m_currentAzVelocityCmd), m_currentElVelocityCmd);
    qDebug() << "Joystick El Value  & desiredElVelocity " << data.joystickElValue << " | " <<  m_currentElVelocityCmd;

    //double desiredAzVelocity = (data.joystickAzValue != 0.0) ? std::copysign(maxSpeedDegS, data.joystickAzValue) : 0.0;
    //double desiredElVelocity = (data.joystickElValue != 0.0) ? std::copysign(maxSpeedDegS, data.joystickElValue) : 0.0;
   
    // Pass the desired world velocity to the base class.
    // It will handle stabilization, limit checks, and hardware communication.
     //   sendStabilizedServoCommands(controller, 0.0, 0.0);
    //sendStabilizedServoCommands(controller, desiredAzVelocity, desiredElVelocity);
    //LOG_TS_ELAPSED("ManualMotionMode", "Processed ManualMotionMode");

*/

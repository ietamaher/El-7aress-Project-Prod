#include "gimbalmotionmodebase.h"
#include "../gimbalcontroller.h"
#include "hardware/devices/servodriverdevice.h"
#include "models/domain/systemstatemodel.h"
#include "safety/SafetyInterlock.h"
#include <QDebug>
#include <algorithm>

// ============================================================================
// STATIC MEMBER DEFINITIONS
// ============================================================================
GimbalStabilizer GimbalMotionModeBase::s_stabilizer;

// ============================================================================
// STATIC PACKET TEMPLATES (SINGLE MODBUS WRITE OPTIMIZATION)
// ============================================================================
// Pre-built 10-register packets for axis-specific velocity commands.
// Only speed (registers 0-1) changes at runtime; accel/decel/current/trigger are fixed.
QVector<quint16> GimbalMotionModeBase::s_azVelocityPacketTemplate;
QVector<quint16> GimbalMotionModeBase::s_elVelocityPacketTemplate;
bool GimbalMotionModeBase::s_packetsInitialized = false;

// ============================================================================
// REGISTER DEFINITIONS for AZD-KX Direct Data Operation (from your manual)
// ============================================================================
namespace AzdReg {
    constexpr quint16 OpType      = 0x005A; // Operation Type (2 registers)
    constexpr quint16 OpPosition  = 0x005C; // Position (2 registers)
    constexpr quint16 OpSpeed     = 0x005E; // Operating Speed (2 registers)
    constexpr quint16 OpAccel     = 0x0060; // Accel rate (2 registers)
    constexpr quint16 OpDecel     = 0x0062; // Decel rate (2 registers)
    constexpr quint16 OpCurrent   = 0x0064; // Operating current (2 registers)
    constexpr quint16 OpTrigger   = 0x0066; // Trigger (2 registers)

    // Trigger values
    constexpr qint32 TRIGGER_UPDATE_SPEED = -4;  // 0xFFFFFFFC
    constexpr qint32 TRIGGER_UPDATE_ALL   = 1;
}


void GimbalMotionModeBase::configureVelocityMode(ServoDriverDevice* driverInterface)
{
    if (!driverInterface) return;

    // Ensure packet templates are initialized (lazy init for safety)
    if (!s_packetsInitialized) {
        initAxisPacketTemplates();
    }

    // This function sets up the driver for continuous speed control.
    // It should be called from the enterMode() of each motion class.

    // Set Operation Type to 16: Continuous operation (speed control)
    QVector<quint16> opTypeData = {0x0000, 0x0010}; // 16 is 0x10
    driverInterface->writeData(AzdReg::OpType, opTypeData);

    // NOTE: Accel/Decel/Current are now set per-write in writeVelocityCommandOptimized()
    // This allows axis-specific values without needing to track which axis this driver is.
    // The initial write will set all parameters correctly.
}

// ============================================================================
// PACKET TEMPLATE INITIALIZATION (call once at startup)
// ============================================================================
void GimbalMotionModeBase::initAxisPacketTemplates()
{
    if (s_packetsInitialized) {
        return;  // Already initialized
    }

    const auto& cfg = MotionTuningConfig::instance();
    constexpr qint32 trigger = AzdReg::TRIGGER_UPDATE_SPEED;  // -4

    // ============================================================================
    // BUILD AZIMUTH PACKET TEMPLATE
    // Heavy turret load: SLOW decel (100kHz/s) to prevent overvoltage
    // ============================================================================
    quint32 azAccel = cfg.axisServo.azimuth.accelHz;
    quint32 azDecel = cfg.axisServo.azimuth.decelHz;
    quint32 azCurrent = cfg.axisServo.azimuth.currentPercent;

    s_azVelocityPacketTemplate.clear();
    s_azVelocityPacketTemplate.reserve(10);
    s_azVelocityPacketTemplate << 0 << 0;  // Speed placeholder [0-1]
    s_azVelocityPacketTemplate << static_cast<quint16>((azAccel >> 16) & 0xFFFF)
                               << static_cast<quint16>(azAccel & 0xFFFF);
    s_azVelocityPacketTemplate << static_cast<quint16>((azDecel >> 16) & 0xFFFF)
                               << static_cast<quint16>(azDecel & 0xFFFF);
    s_azVelocityPacketTemplate << static_cast<quint16>((azCurrent >> 16) & 0xFFFF)
                               << static_cast<quint16>(azCurrent & 0xFFFF);
    s_azVelocityPacketTemplate << static_cast<quint16>((trigger >> 16) & 0xFFFF)
                               << static_cast<quint16>(trigger & 0xFFFF);

    // ============================================================================
    // BUILD ELEVATION PACKET TEMPLATE
    // Lighter gun load: FAST decel (300kHz/s) for crisp stops, 70% current
    // ============================================================================
    quint32 elAccel = cfg.axisServo.elevation.accelHz;
    quint32 elDecel = cfg.axisServo.elevation.decelHz;
    quint32 elCurrent = cfg.axisServo.elevation.currentPercent;

    s_elVelocityPacketTemplate.clear();
    s_elVelocityPacketTemplate.reserve(10);
    s_elVelocityPacketTemplate << 0 << 0;  // Speed placeholder [0-1]
    s_elVelocityPacketTemplate << static_cast<quint16>((elAccel >> 16) & 0xFFFF)
                               << static_cast<quint16>(elAccel & 0xFFFF);
    s_elVelocityPacketTemplate << static_cast<quint16>((elDecel >> 16) & 0xFFFF)
                               << static_cast<quint16>(elDecel & 0xFFFF);
    s_elVelocityPacketTemplate << static_cast<quint16>((elCurrent >> 16) & 0xFFFF)
                               << static_cast<quint16>(elCurrent & 0xFFFF);
    s_elVelocityPacketTemplate << static_cast<quint16>((trigger >> 16) & 0xFFFF)
                               << static_cast<quint16>(trigger & 0xFFFF);

    s_packetsInitialized = true;

    qInfo() << "[GimbalMotionModeBase] Axis packet templates initialized:";
    qInfo() << "  AZ: accel=" << azAccel << "Hz/s, decel=" << azDecel
            << "Hz/s, current=" << azCurrent / 10.0 << "%";
    qInfo() << "  EL: accel=" << elAccel << "Hz/s, decel=" << elDecel
            << "Hz/s, current=" << elCurrent / 10.0 << "%";
}

// ============================================================================
// OPTIMIZED VELOCITY COMMAND (SINGLE MODBUS WRITE)
// ============================================================================
void GimbalMotionModeBase::writeVelocityCommandOptimized(
    ServoDriverDevice* driverInterface,
    GimbalAxis axis,
    double finalVelocity,
    double scalingFactor,
    qint32& lastSpeedHz)
{
    if (!driverInterface) return;

    // Lazy initialization if not done at startup
    if (!s_packetsInitialized) {
        initAxisPacketTemplates();
    }

    // ⚡ Apply axis-specific max speed scaling
    // Elevation is typically limited to 70% of azimuth max speed (RCWS spec)
    const auto& cfg = MotionTuningConfig::instance();
    double speedScale = (axis == GimbalAxis::Azimuth)
        ? cfg.axisServo.azimuth.maxSpeedScale
        : cfg.axisServo.elevation.maxSpeedScale;

    double scaledVelocity = finalVelocity * speedScale;
    qint32 speedHz = static_cast<qint32>(std::lround(scaledVelocity * scalingFactor));

    if (speedHz != lastSpeedHz) {
        // Select the appropriate pre-built template
        QVector<quint16>& packetTemplate = (axis == GimbalAxis::Azimuth)
            ? s_azVelocityPacketTemplate
            : s_elVelocityPacketTemplate;

        // Copy template and fill in speed (only first 2 registers change)
        QVector<quint16> packet = packetTemplate;
        QVector<quint16> speedData = splitInt32ToRegs(speedHz);
        packet[0] = speedData[0];
        packet[1] = speedData[1];

        // ⚡ SINGLE Modbus write: Speed→Accel→Decel→Current→Trigger (10 registers)
        driverInterface->writeData(AzdReg::OpSpeed, packet);

        lastSpeedHz = speedHz;
    }
}

// ============================================================================
// LEGACY VELOCITY COMMAND (for backward compatibility - uses AZ template)
// ============================================================================
void GimbalMotionModeBase::writeVelocityCommand(ServoDriverDevice* driverInterface,
                                                double finalVelocity,
                                                double scalingFactor,
                                                qint32& lastSpeedHz)
{
    // Legacy function - defaults to azimuth parameters for backward compatibility
    // New code should use writeVelocityCommandOptimized() with explicit axis
    writeVelocityCommandOptimized(driverInterface, GimbalAxis::Azimuth,
                                  finalVelocity, scalingFactor, lastSpeedHz);
}

void GimbalMotionModeBase::updateGyroBias(const SystemStateData& systemState)
{
    // Static variables to maintain state across calls
    static double sumX = 0, sumY = 0, sumZ = 0;  // ← CHANGED: Added X and Y
    static int count = 0;
    static QDateTime lastResetTime = QDateTime::currentDateTime();

    // Only estimate bias if the vehicle is stationary
    if (systemState.isVehicleStationary) {
        sumX += systemState.GyroX;
        sumY += systemState.GyroY;
        sumZ += systemState.GyroZ;
        count++;

        if (count >= 50) {
            m_gyroBiasX = sumX / count;
            m_gyroBiasY = sumY / count;
            m_gyroBiasZ = sumZ / count;
            sumX = sumY = sumZ = 0;
            count = 0;
            lastResetTime = QDateTime::currentDateTime();
            /*qDebug() << "[Gimbal] New Gyro Bias - X:" << m_gyroBiasX
                     << "Y:" << m_gyroBiasY << "Z:" << m_gyroBiasZ;   */
        }
    } else {
        sumX = sumY = sumZ = 0;
        count = 0;
        lastResetTime = QDateTime::currentDateTime();
    }
}

void GimbalMotionModeBase::sendStabilizedServoCommands(GimbalController* controller,
                                 double desiredAzVelocity,
                                 double desiredElVelocity,
                                 bool enableStabilization,
                                 double dt)
{
    // --- Shutdown safety check ---
    if (!controller || !controller->systemStateModel()) {
        // During shutdown, systemStateModel may be destroyed before controller
        // Just return silently - no commands needed during cleanup
        return;
    }

    // --- Step 1: Get current system state ---
    SystemStateData systemState = controller->systemStateModel()->data();

    double finalAzVelocity = desiredAzVelocity;
    double finalElVelocity = desiredElVelocity;

    // Debug: show inputs
    /*qDebug().nospace() << "[DBG SEND] in: desiredAz=" << desiredAzVelocity
                       << " desiredEl=" << desiredElVelocity
                       << " enableStab=" << enableStabilization
                       << " encAz=" << systemState.gimbalAz
                       << " encEl=" << systemState.gimbalEl;*/

    // --- Step 2: Apply stabilization if enabled ---
    // Use debug version to populate stabDebug in system state for OSD visualization
    SystemStateData::StabilizationDebug stabDebug;

    if (enableStabilization && systemState.enableStabilization) {
        auto [stabAz_dps, stabEl_dps] = s_stabilizer.computeStabilizedVelocityWithDebug(
            stabDebug,
            desiredAzVelocity,
            desiredElVelocity,
            systemState.imuRollDeg,
            systemState.imuPitchDeg,
            systemState.imuYawDeg,
            systemState.GyroX,
            systemState.GyroY,
            systemState.GyroZ,
            systemState.gimbalAz,
            systemState.gimbalEl,
            systemState.targetAzimuth_world,
            systemState.targetElevation_world,
            systemState.useWorldFrameTarget,
            dt
        );

        finalAzVelocity = stabAz_dps;
        finalElVelocity =  stabEl_dps;
    } else {
        // Not stabilizing - fill debug with raw values
        stabDebug.userAz_dps = desiredAzVelocity;
        stabDebug.userEl_dps = desiredElVelocity;
        stabDebug.stabActive = false;
        stabDebug.worldTargetHeld = false;
        stabDebug.p_dps = systemState.GyroX;
        stabDebug.q_dps = systemState.GyroY;
        stabDebug.r_dps = -systemState.GyroZ;  // Sign inversion for debug visibility
        stabDebug.finalAz_dps = finalAzVelocity;
        stabDebug.finalEl_dps = finalElVelocity;
    }

    // Update system state with debug info for OSD display
    controller->systemStateModel()->updateStabilizationDebug(stabDebug);

    // Step 3: limits
    finalAzVelocity = qBound(-MAX_VELOCITY(), finalAzVelocity, MAX_VELOCITY());
    finalElVelocity = qBound(-MAX_VELOCITY(), finalElVelocity, MAX_VELOCITY());

    // ----------------------------
    // NO-TRAVERSE ENFORCEMENT (improved: clamp to boundary to avoid overshoot)
    // ----------------------------
    if (controller && controller->systemStateModel()) {
        auto *ssm = controller->systemStateModel();

        float currentAz = ssm->data().gimbalAz;
        float currentEl = ssm->data().gimbalEl;

                // Intended deltas (deg) for this control cycle
        float allowedAzDelta = 0.0f;
        float allowedElDelta = 0.0f;

        // 1. Calculate Intent
        float intendedAzDelta = finalAzVelocity * dt;
        float intendedElDelta = finalElVelocity * dt;

        // 2. Compute Physics (Modifies allowed deltas independently)
        ssm->computeAllowedDeltas(
            currentAz, currentEl,
            intendedAzDelta, intendedElDelta,
            allowedAzDelta, allowedElDelta,
            dt
        );

        // 3. Convert back to Velocity (Per Axis)
        if (dt > 1e-6) {
            finalAzVelocity = allowedAzDelta / dt;
            finalElVelocity = allowedElDelta / dt;
        } else {
            finalAzVelocity = 0;
            finalElVelocity = 0;
        }

        // Optional debug/logging (throttle)
        static int ntzLog = 0;
        if ((++ntzLog % 40) == 0) {
            if (!qFuzzyCompare(static_cast<float>(intendedAzDelta), finalAzVelocity * dt) ||
                !qFuzzyCompare(static_cast<float>(intendedElDelta), finalElVelocity * dt)) {
                qDebug() << "[Gimbal] NTZ clamp: intendedAzDelta" << intendedAzDelta
                         << "allowedAzDelta" << allowedAzDelta
                         << "intendedElDelta" << intendedElDelta
                         << "allowedElDelta" << allowedElDelta;
            }
        }
    }

    // --- Step 4: Convert to servo steps and send commands (AZD-KD velocity mode) ---
    // ⚡ OPTIMIZED: Uses axis-specific packet templates with different accel/decel/current
    // - Azimuth: Slow decel (100kHz/s) to prevent overvoltage on heavy turret
    // - Elevation: Fast decel (300kHz/s) for crisp stops, 70% current limit
    if (auto azServo = controller->azimuthServo()) {
        writeVelocityCommandOptimized(azServo, GimbalAxis::Azimuth,
                                      finalAzVelocity, AZ_STEPS_PER_DEGREE(), m_lastAzSpeedHz);
    }
    if (auto elServo = controller->elevationServo()) {
        writeVelocityCommandOptimized(elServo, GimbalAxis::Elevation,
                                      finalElVelocity, EL_STEPS_PER_DEGREE(), m_lastElSpeedHz);
    }
}


double GimbalMotionModeBase::pidCompute(PIDController& pid, double error, double setpoint, double measurement, bool derivativeOnMeasurement, double dt)
{
    // Proportional term
    double proportional = pid.Kp * error;

    // Integral term with windup protection
    pid.integral += error * dt;
    pid.integral = qBound(-pid.maxIntegral, pid.integral, pid.maxIntegral);
    double integral = pid.Ki * pid.integral;

    // Derivative term
    double derivative = 0.0;
    if (dt > 1e-6) {
        if (derivativeOnMeasurement) {
            // Derivative on Measurement: avoids "kick" on setpoint changes.
            // Note the negative sign: the derivative must oppose the direction of change.
            derivative = -pid.Kd * (measurement - pid.previousMeasurement) / dt;
        } else {
            // Derivative on Error: the classic implementation.
            derivative = pid.Kd * (error - pid.previousError) / dt;
        }
    }

    // Store history for the next cycle
    pid.previousError = error;
    pid.previousMeasurement = measurement;

    return proportional + integral + derivative;
}

// Implementation of the original, simpler PID function (overload)
// This function now calls the more advanced one with the correct parameters.
double GimbalMotionModeBase::pidCompute(PIDController& pid, double error, double dt)
{
    // We call the main function with dummy values for setpoint/measurement
    // and explicitly set derivativeOnMeasurement to false.
    // Setpoint and measurement are not used when derivativeOnMeasurement is false,
    // so their values don't matter.
    return pidCompute(pid, error, 0.0, 0.0, false, dt);
}


void GimbalMotionModeBase::stopServos(GimbalController* controller)
{
    // Shutdown safety: check both controller and systemStateModel
    if (!controller || !controller->systemStateModel()) {
        // During shutdown, just return - no commands needed
        return;
    }
        // ⭐ FIX: Force write by invalidating last speed cache
    //m_lastAzSpeedHz = INT32_MAX;  // Impossible value forces write
    //m_lastElSpeedHz = INT32_MAX;
    // Send a zero-velocity command through the new architecture.
    // Disable stabilization when stopping (no need to hold position)
    const double dt = 0.05; // 50ms nominal, doesn't matter for zero velocity
    sendStabilizedServoCommands(controller, 0.0, 0.0, false, dt);
}

void GimbalMotionModeBase::writeServoCommands(ServoDriverDevice* driverInterface, double finalVelocity, float scalingFactor)
{
    if (!driverInterface) return;

    // Determine direction from the sign of the final velocity
    quint16 direction;
    if (finalVelocity > 0.01) { // Added deadband
        direction = DIRECTION_REVERSE; // This mapping depends on wiring
    } else if (finalVelocity < -0.01) {
        direction = DIRECTION_FORWARD;
    } else {
        direction = DIRECTION_STOP;
    }

    // Calculate speed (magnitude of velocity) scaled to servo units
    quint32 speedCommand = static_cast<quint32>(std::abs(finalVelocity) * scalingFactor);
    quint32 clampedSpeed = std::min(speedCommand, MAX_SPEED);

    // Split speed into two 16-bit registers
    quint16 upperBits = static_cast<quint16>((clampedSpeed >> 16) & 0xFFFF);
    quint16 lowerBits = static_cast<quint16>(clampedSpeed & 0xFFFF);

    driverInterface->writeData(SPEED_REGISTER, {upperBits, lowerBits});
    driverInterface->writeData(DIRECTION_REGISTER, {direction});
}


void GimbalMotionModeBase::writeTargetPosition(ServoDriverDevice* driverInterface,
                                             long targetPositionInSteps)
{
    if (!driverInterface) return;

    // Oriental Motor drivers often take a 32-bit position.
    // We need to split it into two 16-bit registers.
    quint16 upperSteps = static_cast<quint16>((targetPositionInSteps >> 16) & 0xFFFF);
    quint16 lowerSteps = static_cast<quint16>(targetPositionInSteps & 0xFFFF);

    // PSEUDO-CODE: Register addresses would come from the AZD-KX manual.
    static constexpr quint16 TARGET_POS_UPPER_REG = 0x0100;
    static constexpr quint16 TARGET_POS_LOWER_REG = 0x0102;
    static constexpr quint16 EXECUTE_MOVE_REG     = 0x007D;

    // Write the new target position
    driverInterface->writeData(TARGET_POS_UPPER_REG, {upperSteps});
    driverInterface->writeData(TARGET_POS_LOWER_REG, {lowerSteps});

    // Trigger the move
    driverInterface->writeData(EXECUTE_MOVE_REG, {0x0001}); // "Start Move" command
}


void GimbalMotionModeBase::setAcceleration(ServoDriverDevice* driverInterface, quint32 acceleration)
{
    if (!driverInterface) return;

    quint32 clamped = std::min(acceleration, MAX_ACCELERATION);
    quint16 upper = static_cast<quint16>((clamped >> 16) & 0xFFFF);
    quint16 lower = static_cast<quint16>(clamped & 0xFFFF);

    QVector<quint16> accelData = {upper, lower};

    // Write to all acceleration registers
    for (quint16 reg : ACCEL_REGISTERS) {
        driverInterface->writeData(reg, accelData);
    }
}

bool GimbalMotionModeBase::checkSafetyConditions(GimbalController* controller)
{
    if (!controller) {
        return false;
    }

    // ============================================================================
    // SAFETY INTERLOCK INTEGRATION (Template Method Pattern)
    // ============================================================================
    // Delegate safety decision to centralized SafetyInterlock authority.
    // This ensures all safety checks are in one place and auditable.
    // The SafetyInterlock.canMove() checks:
    //   - E-Stop status
    //   - Station enabled
    //   - Dead man switch (for Manual/AutoTrack modes)
    // ============================================================================
    if (controller->safetyInterlock()) {
        SafetyDenialReason reason;
        int motionMode = static_cast<int>(controller->currentMotionModeType());
        bool allowed = controller->safetyInterlock()->canMove(motionMode, &reason);
        if (!allowed) {
            static int logCounter = 0;
            if (++logCounter % 100 == 1) {  // Throttle logging (every 5 seconds at 20Hz)
                qDebug() << "[GimbalMotionModeBase] Motion denied by SafetyInterlock:"
                         << SafetyInterlock::denialReasonToString(reason);
            }
        }
        return allowed;
    }

    // ============================================================================
    // LEGACY FALLBACK (if SafetyInterlock not available)
    // ============================================================================
    if (!controller->systemStateModel()) {
        return false;
    }

    SystemStateData data = controller->systemStateModel()->data();

    // Dead man switch required for Manual, AutoTrack, and ManualTrack modes
    bool deadManSwitchOk = true;
    MotionMode mode = controller->currentMotionModeType();
    if (mode == MotionMode::Manual ||
        mode == MotionMode::AutoTrack ||
        mode == MotionMode::ManualTrack) {
        deadManSwitchOk = data.deadManSwitchActive;
    }

    return data.stationEnabled &&
           !data.emergencyStopActive && deadManSwitchOk;
}

// ============================================================================
// TEMPLATE METHOD PATTERN IMPLEMENTATION
// ============================================================================
// These methods enforce safety checks before any motion update.
// The pattern guarantees that all motion passes through SafetyInterlock.

void GimbalMotionModeBase::updateWithSafety(GimbalController* controller, double dt)
{
    // Step 1: Check safety conditions via SafetyInterlock
    if (!checkSafetyConditions(controller)) {
        // Safety denied - stop all motion immediately
        stopServos(controller);
        return;
    }

    // Step 2: Safety passed - delegate to implementation
    updateImpl(controller, dt);
}

void GimbalMotionModeBase::update(GimbalController* controller, double dt)
{
    // Delegate to updateWithSafety() for guaranteed safety enforcement
    // This preserves backward compatibility while ensuring safety
    updateWithSafety(controller, dt);
}

void GimbalMotionModeBase::updateImpl(GimbalController* /*controller*/, double /*dt*/)
{
    // Default implementation - no motion
    // Derived classes override this to provide their specific motion logic
}

bool GimbalMotionModeBase::checkElevationLimits(double currentEl, double targetVelocity,
                                               bool upperLimit, bool lowerLimit)
{
    // Check upper limits
    if ((currentEl >= MAX_ELEVATION_ANGLE || upperLimit) && (targetVelocity > 0)) {
        return false;
    }

    // Check lower limits
    if ((currentEl <= MIN_ELEVATION_ANGLE || lowerLimit) && (targetVelocity < 0)) {
        return false;
    }

    return true;
}

// ============================================================================
// DEPRECATED FUNCTIONS REMOVED (2025-12-27)
// The following functions were removed as they are replaced by GimbalStabilizer:
//   - calculateStabilizationCorrection()
//   - calculateRequiredGimbalAngles()
// Production code should use GimbalStabilizer::computeStabilizedVelocity()
// ============================================================================


void GimbalMotionModeBase::convertGimbalToWorldFrame(
    double gimbalAz_platform, double gimbalEl_platform,
    double platform_roll, double platform_pitch, double platform_yaw,
    double& worldAz, double& worldEl)
{
    // Convert angles to radians
    double gAz = degToRad(gimbalAz_platform);
    double gEl = degToRad(gimbalEl_platform);
    double roll = degToRad(platform_roll);
    double pitch = degToRad(platform_pitch);
    double yaw = degToRad(platform_yaw);

    // STEP A: Create unit vector from gimbal angles in platform frame
    double cos_gEl = cos(gEl);
    double x_platform = cos_gEl * cos(gAz);
    double y_platform = cos_gEl * sin(gAz);
    double z_platform = sin(gEl);

    // STEP B: Rotate vector FROM platform frame TO world frame (forward rotation: XYZ)

    // Apply roll rotation (X-axis)
    double cos_roll = cos(roll);
    double sin_roll = sin(roll);
    double y_temp1 = y_platform * cos_roll - z_platform * sin_roll;
    double z_temp1 = y_platform * sin_roll + z_platform * cos_roll;
    double x_temp1 = x_platform;

    // Apply pitch rotation (Y-axis)
    double cos_pitch = cos(pitch);
    double sin_pitch = sin(pitch);
    double x_temp2 = x_temp1 * cos_pitch + z_temp1 * sin_pitch;
    double y_temp2 = y_temp1;
    double z_temp2 = -x_temp1 * sin_pitch + z_temp1 * cos_pitch;

    // Apply yaw rotation (Z-axis)
    double cos_yaw = cos(yaw);
    double sin_yaw = sin(yaw);
    double x_world = x_temp2 * cos_yaw - y_temp2 * sin_yaw;
    double y_world = x_temp2 * sin_yaw + y_temp2 * cos_yaw;
    double z_world = z_temp2;

    // STEP C: Convert world-frame vector back to azimuth/elevation angles
    worldAz = radToDeg(atan2(y_world, x_world));
    worldEl = radToDeg(atan2(z_world, sqrt(x_world * x_world + y_world * y_world)));

    // ✅ EXPERT REVIEW FIX: Normalize azimuth to [-180, 180] for consistency with control loops
    worldAz = normalizeAngle180(worldAz);
}

// calculateHybridStabilizationCorrection() - REMOVED (replaced by GimbalStabilizer)

// ============================================================================
// GIMBAL TO WORLD FRAME CONVERSION (Eigen-based rotation matrices)
// ============================================================================

void GimbalMotionModeBase::convertGimbalToWorldFrame(
    const Eigen::Vector3d& linVel_gimbal_mps,
    const Eigen::Vector3d& angVel_gimbal_dps,
    double azDeg,
    double elDeg,
    Eigen::Vector3d& linVel_world_mps,
    Eigen::Vector3d& angVel_world_dps)
{
    // ============================================================================
    // 1. Convert angles to radians
    // ============================================================================
    const double azRad = degToRad(azDeg);   // [rad]
    const double elRad = degToRad(elDeg);   // [rad]

    // ============================================================================
    // 2. Rotation matrix: Gimbal → World
    //
    // Azimuth-over-Elevation gimbal:
    //   World = R_az(Z) * R_el(Y) * Gimbal
    //
    // Rotation order:
    //   1. R_el: Rotate around Y-axis (elevation) - INNER gimbal
    //   2. R_az: Rotate around Z-axis (azimuth) - OUTER gimbal
    // ============================================================================
    const Eigen::Matrix3d R_az = Eigen::AngleAxisd(azRad, Eigen::Vector3d::UnitZ()).toRotationMatrix();
    const Eigen::Matrix3d R_el = Eigen::AngleAxisd(elRad, Eigen::Vector3d::UnitY()).toRotationMatrix();
    const Eigen::Matrix3d R_g2w = R_az * R_el;  // Gimbal-to-World rotation

    // ============================================================================
    // 3. Linear Velocity Transform
    // v_world = R_g2w * v_gimbal
    // Units: m/s → m/s (unchanged)
    // ============================================================================
    linVel_world_mps = R_g2w * linVel_gimbal_mps;

    // ============================================================================
    // 4. Angular Velocity Transform
    // ω_world = R_g2w * ω_gimbal
    // Units: deg/s → rad/s → rotate → deg/s
    // ============================================================================
    const Eigen::Vector3d angVel_gimbal_radps = degToRad(angVel_gimbal_dps);  // deg/s → rad/s
    const Eigen::Vector3d angVel_world_radps = R_g2w * angVel_gimbal_radps;   // Rotate (rad/s)
    angVel_world_dps = radToDeg(angVel_world_radps);                          // rad/s → deg/s
}


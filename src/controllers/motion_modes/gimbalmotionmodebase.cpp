#include "gimbalmotionmodebase.h"
#include "../gimbalcontroller.h"
#include "hardware/devices/servodriverdevice.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <algorithm>

// =========================================================================
// STATIC MEMBER DEFINITIONS
// =========================================================================
GimbalStabilizer GimbalMotionModeBase::s_stabilizer;

// =========================================================================
// REGISTER DEFINITIONS for AZD-KX Direct Data Operation (from your manual)
// =========================================================================
namespace AzdReg {
    // These are all 16-bit register addresses
    constexpr quint16 OpType      = 0x005A; // Operation Type (2 registers)
    constexpr quint16 OpSpeed     = 0x005E; // Operating Speed (2 registers, signed +/- 4,000,000 Hz)
    constexpr quint16 OpAccel     = 0x0060; // Starting/Changing Speed Rate (2 registers)
    constexpr quint16 OpDecel     = 0x0062; // Stopping Deceleration (2 registers)
    constexpr quint16 OpTrigger   = 0x0066; // Trigger (2 registers)
}
 

void GimbalMotionModeBase::configureVelocityMode(ServoDriverDevice* driverInterface)
{
    if (!driverInterface) return;

    // This function sets up the driver for continuous speed control.
    // It should be called from the enterMode() of each motion class.

    // 1. Set Operation Type to 16: Continuous operation (speed control)
    QVector<quint16> opTypeData = {0x0000, 0x0010}; // 16 is 0x10
    driverInterface->writeData(AzdReg::OpType, opTypeData);

    // 2. Set a reasonable default acceleration/deceleration rate.
    // From manual, 1,000,000 = 1000 kHz/s. Let's set 1500 kHz/s = 1,500,000
    quint32 accel = 150000;//0;
    QVector<quint16> accelData = {
        static_cast<quint16>((accel >> 16) & 0xFFFF),
        static_cast<quint16>(accel & 0xFFFF)
    };
    driverInterface->writeData(AzdReg::OpAccel, accelData);
    driverInterface->writeData(AzdReg::OpDecel, accelData); // Use same for decel
}

void GimbalMotionModeBase::writeVelocityCommand(ServoDriverDevice* driverInterface,
                                              double finalVelocity,
                                              double scalingFactor,
                                              qint32& lastSpeedHz)
{
    if (!driverInterface) return;

    // 1. Convert physical velocity (deg/s) to motor speed (Hz) as a signed 32-bit
    // Use lround for proper rounding instead of truncation
    qint32 speedHz = static_cast<qint32>(std::lround(finalVelocity * scalingFactor));

    // 2. ✅ CRITICAL FIX: Only write if speed actually changed!
    // This prevents redundant Modbus writes (was 60 ops/sec, now 2-6 ops/sec)
    if (speedHz != lastSpeedHz) {
        // Do bit-preserving conversion to two 16-bit registers using central helper
        QVector<quint16> speedData = splitInt32ToRegs(speedHz);
        driverInterface->writeData(AzdReg::OpSpeed, speedData);

        // 3. Trigger the speed update (preserve existing trigger sequence)
        // From manual, trigger value -4 (FFFF FFFCh) updates the operating speed
        QVector<quint16> triggerData = {0xFFFF, 0xFFFC};
        driverInterface->writeData(AzdReg::OpTrigger, triggerData);

        lastSpeedHz = speedHz;  // Update last command
    }
    // else: Speed unchanged, skip redundant Modbus write (latency fix!)
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
            qDebug() << "[Gimbal] New Gyro Bias - X:" << m_gyroBiasX 
                     << "Y:" << m_gyroBiasY << "Z:" << m_gyroBiasZ;   
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
    // --- Step 1: Get current system state ---
    SystemStateData systemState = controller->systemStateModel()->data();

    double finalAzVelocity = desiredAzVelocity;
    double finalElVelocity = desiredElVelocity;

    // --- Step 2: Apply stabilization if enabled ---
    // FLAG SEMANTICS:
    //   enableStabilization (param): Mode-level stabilization request (e.g., tracking wants it, manual doesn't)
    //   systemState.enableStabilization: User-level global stabilization toggle (GUI button)
    //   Both must be true to apply stabilization (AND logic)
    if (enableStabilization && systemState.enableStabilization) {
        // ✅ NEW ARCHITECTURE: Use velocity-based GimbalStabilizer
        // Control Law: ω_cmd = ω_user + ω_feedforward + Kp × (angle_error)
        //
        // When stabilization is active, we ALWAYS enable world-frame target holding
        // (position correction + rate feed-forward). The stabilizer handles both
        // AHRS-based drift compensation and gyro-based transient compensation.
        auto [stabAz_dps, stabEl_dps] = s_stabilizer.computeStabilizedVelocity(
            desiredAzVelocity,                      // User-commanded velocity
            desiredElVelocity,
            systemState.imuRollDeg,                 // Platform attitude (AHRS)
            systemState.imuPitchDeg,
            systemState.imuYawDeg,
            systemState.GyroX,                      // Platform rates (gyros)
            systemState.GyroY,
            systemState.GyroZ,
            systemState.gimbalAz,                   // Current gimbal position
            systemState.gimbalEl,
            systemState.targetAzimuth_world,        // World-frame target
            systemState.targetElevation_world,
            true,                                   // Always enable world target holding when stabilizing
            dt
        );

        finalAzVelocity = stabAz_dps;
        finalElVelocity = stabEl_dps;
    }

    // --- Step 3: Apply system-wide velocity limits (from config) ---
    finalAzVelocity = qBound(-MAX_VELOCITY(), finalAzVelocity, MAX_VELOCITY());
    finalElVelocity = qBound(-MAX_VELOCITY(), finalElVelocity, MAX_VELOCITY());

    // --- Step 4: Convert to servo steps and send commands (AZD-KD velocity mode) ---
    // Send velocity commands to AZD-KD drivers (Operation Type 16)
    // Now with change detection to prevent redundant Modbus writes!
    if (auto azServo = controller->azimuthServo()) {
        writeVelocityCommand(azServo, -finalAzVelocity, AZ_STEPS_PER_DEGREE(), m_lastAzSpeedHz);
    }
    if (auto elServo = controller->elevationServo()) {
        writeVelocityCommand(elServo, -finalElVelocity, EL_STEPS_PER_DEGREE(), m_lastElSpeedHz);
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
    if (!controller) return;
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
    if (!controller || !controller->systemStateModel()) {
        return false;
    }
    
    SystemStateData data = controller->systemStateModel()->data();

        bool deadManSwitchOk = true;
    if (controller->currentMotionModeType() == MotionMode::Manual ||
        controller->currentMotionModeType() == MotionMode::AutoTrack) {
        deadManSwitchOk = data.deadManSwitchActive;
    }

    return data.stationEnabled && // data.hatchState &&
           !data.emergencyStopActive &&   deadManSwitchOk;
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

void GimbalMotionModeBase::calculateStabilizationCorrection(double currentAz_deg, double currentEl_deg,
                                                            double gyroX_dps_raw, double gyroY_dps_raw, double gyroZ_dps_raw,
                                                            double& azCorrection_dps, double& elCorrection_dps)
{
    // === START OF DIAGNOSTIC LOGGING ===
    // Use a static variable to log only every Nth frame to avoid flooding the console.
    static int logCounter = 0;
    const int LOG_INTERVAL = 50; // Log every 5th call (20 times per second)

    bool shouldLog = (logCounter++ % LOG_INTERVAL == 0); 

    // Validate inputs
    if (std::isnan(gyroX_dps_raw) || std::isnan(gyroY_dps_raw) || std::isnan(gyroZ_dps_raw) ||
        std::isnan(currentAz_deg) || std::isnan(currentEl_deg)) {
        azCorrection_dps = 0.0;
        elCorrection_dps = 0.0;
        return;
    }
    // Apply bias correction
    double gyroX_dps_corrected = gyroX_dps_raw - m_gyroBiasX;
    double gyroY_dps_corrected = gyroY_dps_raw - m_gyroBiasY;
    double gyroZ_dps_corrected = gyroZ_dps_raw - m_gyroBiasZ;

    // ✅ EXPERT REVIEW FIX: Use updateWithDt() with default dt (this function is legacy, not called)
    double dt = UPDATE_INTERVAL_S();
    double gyroX_dps_filtered = m_gyroXFilter.updateWithDt(gyroX_dps_corrected, dt);
    double gyroY_dps_filtered = m_gyroYFilter.updateWithDt(gyroY_dps_corrected, dt);
    double gyroZ_dps_filtered = m_gyroZFilter.updateWithDt(gyroZ_dps_corrected, dt);

    // ✅ EXPERT REVIEW FIX: Correct IMU→body-rate mapping (unified with calculateHybridStabilizationCorrection)
    // IMU X → p (roll rate), Y → q (pitch rate), Z → r (yaw rate, inverted)
    const double p_imu = gyroX_dps_filtered;  // Roll rate (deg/s)
    const double q_imu = gyroY_dps_filtered;  // Pitch rate (deg/s)
    const double r_imu = -gyroZ_dps_filtered; // Yaw rate (deg/s) - NEGATED because Z is DOWN

    // Get current gimbal angles in radians
    const double currentAzRad = degToRad(currentAz_deg);
    const double currentElRad = degToRad(currentEl_deg);

    // Apply full kinematic transformation
    double platformEffectOnEl = (q_imu * cos(currentAzRad)) - (p_imu * sin(currentAzRad));

    double tanEl = tan(currentElRad); // Calculate tan() once
    double platformEffectOnAz;
    if (qAbs(cos(currentElRad)) < 1e-6) {
        platformEffectOnAz = r_imu;
    } else {
        platformEffectOnAz = r_imu + tanEl * (q_imu * sin(currentAzRad) + p_imu * cos(currentAzRad));
    }

    // The correction is the negative of the platform's effect
    azCorrection_dps = -platformEffectOnAz;
    elCorrection_dps = -platformEffectOnEl;

    // --- DETAILED LOGGING OUTPUT ---
    if (shouldLog) {
        // Use fixed-point notation for easy comparison
        qDebug().noquote().nospace()
            << "StabIn(GYRO):"
            << " X=" << QString::number(gyroX_dps_raw, 'f', 2)
            << " Y=" << QString::number(gyroY_dps_raw, 'f', 2)
            << " Z=" << QString::number(gyroZ_dps_raw, 'f', 2)
            << " | ElAngle=" << QString::number(currentEl_deg, 'f', 1)
            << " tan(El)=" << QString::number(tanEl, 'f', 2)
            << " | StabOut(CORR):"
            << " Az=" << QString::number(azCorrection_dps, 'f', 2)
            << " El=" << QString::number(elCorrection_dps, 'f', 2);
    }
    // ================================

    // Limit the correction velocity
    const double MAX_CORRECTION_DPS = 5.0;
    azCorrection_dps = qBound(-MAX_CORRECTION_DPS, azCorrection_dps, MAX_CORRECTION_DPS);
    elCorrection_dps = qBound(-MAX_CORRECTION_DPS, elCorrection_dps, MAX_CORRECTION_DPS);
}

// =========================================================================
// AHRS-BASED WORLD-FRAME STABILIZATION FUNCTIONS
// =========================================================================

void GimbalMotionModeBase::calculateRequiredGimbalAngles(
    double platform_roll, double platform_pitch, double platform_yaw,
    double target_az_world, double target_el_world,
    double& required_gimbal_az, double& required_gimbal_el)
{
    // ✅ EXPERT REVIEW FIX: Matrix-based approach to avoid gimbal lock and sign errors
    //
    // This function computes the gimbal angles needed to point at a specific
    // world-frame target, given the platform's orientation.
    //
    // Mathematical approach:
    // 1. Build platform rotation matrix: Rplat = Rz(yaw) * Ry(pitch) * Rx(roll)
    // 2. Create unit vector pointing at world target
    // 3. Transform to platform frame: v_platform = Rplat^T * v_world
    // 4. Extract gimbal angles from platform-frame vector

    // Convert angles to radians
    double roll  = degToRad(platform_roll);
    double pitch = degToRad(platform_pitch);
    double yaw   = degToRad(platform_yaw);
    double target_az = degToRad(target_az_world);
    double target_el = degToRad(target_el_world);

    // Step 1: Build platform rotation matrix (world -> platform)
    // Rplat = Rz(yaw) * Ry(pitch) * Rx(roll)
    Eigen::Matrix3d Rz;
    Rz <<  cos(yaw), -sin(yaw), 0,
           sin(yaw),  cos(yaw), 0,
                0,         0,   1;

    Eigen::Matrix3d Ry;
    Ry <<  cos(pitch), 0, sin(pitch),
                 0,    1,      0,
          -sin(pitch), 0, cos(pitch);

    Eigen::Matrix3d Rx;
    Rx << 1,      0,         0,
          0, cos(roll), -sin(roll),
          0, sin(roll),  cos(roll);

    Eigen::Matrix3d Rplat = Rz * Ry * Rx;

    // Step 2: Create unit vector pointing at world target
    double cos_el = cos(target_el);
    Eigen::Vector3d v_world;
    v_world << cos_el * cos(target_az),
               cos_el * sin(target_az),
               sin(target_el);

    // Step 3: Transform target vector to platform frame
    Eigen::Vector3d v_platform = Rplat.transpose() * v_world;

    // Step 4: Extract gimbal angles from platform-frame vector
    // Azimuth: angle in XY plane
    // Elevation: angle from XY plane to vector
    required_gimbal_az = radToDeg(atan2(v_platform.y(), v_platform.x()));
    required_gimbal_el = radToDeg(atan2(v_platform.z(),
                                        sqrt(v_platform.x() * v_platform.x() +
                                             v_platform.y() * v_platform.y())));

    // ✅ EXPERT REVIEW FIX: Negate elevation to match system sign convention
    // (Consistent with GimbalStabilizer::computeRequiredGimbalAngles)
    required_gimbal_el = -required_gimbal_el;
}


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

void GimbalMotionModeBase::calculateHybridStabilizationCorrection(
    const SystemStateData& state,
    double& azCorrection_dps,
    double& elCorrection_dps,
    double dt)
{
    // -------------------------
    // Units convention (explicit)
    // - state.GyroX/Y/Z : IMU gyro output in deg/s
    // - p_dps, q_dps, r_dps : body rates in deg/s (consistent)
    // - currentAzRad/currentElRad : angles in radians (for trig)
    // - trig outputs: dimensionless
    // - All corrections (position and velocity) are in deg/s
    // -------------------------

    // LAYER 1: Position (AHRS-based) -> produces corrections in deg/s
    double positionCorrectionAz_dps = 0.0;
    double positionCorrectionEl_dps = 0.0;

    if (state.useWorldFrameTarget && state.imuConnected) {
        double required_az_deg = 0.0;
        double required_el_deg = 0.0;
        calculateRequiredGimbalAngles(
            state.imuRollDeg,
            state.imuPitchDeg,
            state.imuYawDeg,
            state.targetAzimuth_world,
            state.targetElevation_world,
            required_az_deg,
            required_el_deg
        );

        double az_error_deg = required_az_deg - state.gimbalAz;
        double el_error_deg = required_el_deg - state.gimbalEl;

        // Normalize az error to [-180, 180]
        az_error_deg = normalizeAngle180(az_error_deg);

        const double Kp_position = 2.0; // deg/s per degree
        positionCorrectionAz_dps = Kp_position * az_error_deg;
        positionCorrectionEl_dps = Kp_position * el_error_deg;

        const double MAX_POSITION_VEL = 10.0; // deg/s
        positionCorrectionAz_dps = qBound(-MAX_POSITION_VEL, positionCorrectionAz_dps, MAX_POSITION_VEL);
        positionCorrectionEl_dps = qBound(-MAX_POSITION_VEL, positionCorrectionEl_dps, MAX_POSITION_VEL);
    }

    // LAYER 2: Velocity feedforward (gyro-based) - EXPERT REVIEW: DT-AWARE FILTERING
    double velocityCorrectionAz_dps = 0.0;
    double velocityCorrectionEl_dps = 0.0;

    if (state.imuConnected) {
        if (std::isnan(state.GyroX) || std::isnan(state.GyroY) || std::isnan(state.GyroZ)) {
            velocityCorrectionAz_dps = 0.0;
            velocityCorrectionEl_dps = 0.0;
        } else {
            // ✅ EXPERT REVIEW FIX: dt is now passed from GimbalController (centralized measurement)

            // Bias correction (deg/s)
            double gx = state.GyroX - m_gyroBiasX;
            double gy = state.GyroY - m_gyroBiasY;
            double gz = state.GyroZ - m_gyroBiasZ;

            // ✅ EXPERT REVIEW FIX: Filter using runtime dt (passed from GimbalController)
            double gx_f = m_gyroXFilter.updateWithDt(gx, dt);
            double gy_f = m_gyroYFilter.updateWithDt(gy, dt);
            double gz_f = m_gyroZFilter.updateWithDt(gz, dt);

            // ✅ VERIFIED IMU ORIENTATION (2025-11-14):
            // IMU Frame: X forward, Y right, Z down (Forward-Right-Down)
            // Map IMU -> body rates (deg/s)
            const double p_dps = gx_f;       // roll rate (deg/s)
            const double q_dps = gy_f;       // pitch rate (deg/s)
            const double r_dps = -gz_f;      // yaw rate (deg/s) -- NEGATED because Z is DOWN

            // Angles for trig (radians)
            const double azRad = degToRad(state.gimbalAz);
            const double elRad = degToRad(state.gimbalEl);

            // Platform effects (deg/s)
            double platformEffectOnEl_dps = (q_dps * cos(azRad)) - (p_dps * sin(azRad));

            // Robust tan handling with conservative clamp
            const double COS_EPS = 1e-6;
            const double MAX_TAN_EL = 1e3;  // Increased clamp for numerical stability
            double tanEl = 0.0;
            if (qAbs(cos(elRad)) < COS_EPS) {
                tanEl = (elRad >= 0) ? MAX_TAN_EL : -MAX_TAN_EL;
            } else {
                tanEl = tan(elRad);
                if (qAbs(tanEl) > MAX_TAN_EL) tanEl = (tanEl > 0 ? MAX_TAN_EL : -MAX_TAN_EL);
            }

            double platformTerm_degps = (q_dps * sin(azRad)) + (p_dps * cos(azRad)); // deg/s
            double platformEffectOnAz_dps = r_dps + tanEl * platformTerm_degps; // deg/s

            // Corrections are negatives of platform effects
            double azCorr_dps = -platformEffectOnAz_dps;
            double elCorr_dps = -platformEffectOnEl_dps;

            // Limit corrections
            const double MAX_VELOCITY_CORR = 5.0; // deg/s
            velocityCorrectionAz_dps = qBound(-MAX_VELOCITY_CORR, azCorr_dps, MAX_VELOCITY_CORR);
            velocityCorrectionEl_dps = qBound(-MAX_VELOCITY_CORR, elCorr_dps, MAX_VELOCITY_CORR);
        }
    }

    // Combine (both layers in deg/s)
    azCorrection_dps = positionCorrectionAz_dps + velocityCorrectionAz_dps;
    elCorrection_dps = positionCorrectionEl_dps + velocityCorrectionEl_dps;

    // Final cap
    const double MAX_TOTAL_VEL = 12.0; // deg/s
    azCorrection_dps = qBound(-MAX_TOTAL_VEL, azCorrection_dps, MAX_TOTAL_VEL);
    elCorrection_dps = qBound(-MAX_TOTAL_VEL, elCorrection_dps, MAX_TOTAL_VEL);

    // Logging (same behavior as before)
    static int logCounter = 0;
    if ((logCounter++ % 50) == 0 && state.useWorldFrameTarget) {
        qDebug().noquote().nospace()
            << "[HybridStab] TargetWorld: Az=" << QString::number(state.targetAzimuth_world, 'f', 1)
            << "° El=" << QString::number(state.targetElevation_world, 'f', 1)
            << "° | PosCorr: Az=" << QString::number(positionCorrectionAz_dps, 'f', 2)
            << " El=" << QString::number(positionCorrectionEl_dps, 'f', 2)
            << " | VelCorr: Az=" << QString::number(velocityCorrectionAz_dps, 'f', 2)
            << " El=" << QString::number(velocityCorrectionEl_dps, 'f', 2);
    }
}

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
    // ======================================================
    // 1. Convert angles to radians
    // ======================================================
    const double azRad = degToRad(azDeg);   // [rad]
    const double elRad = degToRad(elDeg);   // [rad]

    // ======================================================
    // 2. Rotation matrix: Gimbal → World
    //
    // Azimuth-over-Elevation gimbal:
    //   World = R_az(Z) * R_el(Y) * Gimbal
    //
    // Rotation order:
    //   1. R_el: Rotate around Y-axis (elevation) - INNER gimbal
    //   2. R_az: Rotate around Z-axis (azimuth) - OUTER gimbal
    // ======================================================
    const Eigen::Matrix3d R_az = Eigen::AngleAxisd(azRad, Eigen::Vector3d::UnitZ()).toRotationMatrix();
    const Eigen::Matrix3d R_el = Eigen::AngleAxisd(elRad, Eigen::Vector3d::UnitY()).toRotationMatrix();
    const Eigen::Matrix3d R_g2w = R_az * R_el;  // Gimbal-to-World rotation

    // ======================================================
    // 3. Linear Velocity Transform
    // v_world = R_g2w * v_gimbal
    // Units: m/s → m/s (unchanged)
    // ======================================================
    linVel_world_mps = R_g2w * linVel_gimbal_mps;

    // ======================================================
    // 4. Angular Velocity Transform
    // ω_world = R_g2w * ω_gimbal
    // Units: deg/s → rad/s → rotate → deg/s
    // ======================================================
    const Eigen::Vector3d angVel_gimbal_radps = degToRad(angVel_gimbal_dps);  // deg/s → rad/s
    const Eigen::Vector3d angVel_world_radps = R_g2w * angVel_gimbal_radps;   // Rotate (rad/s)
    angVel_world_dps = radToDeg(angVel_world_radps);                          // rad/s → deg/s
}


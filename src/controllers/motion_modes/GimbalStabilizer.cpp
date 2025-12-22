#include "GimbalStabilizer.h"
#include "config/MotionTuningConfig.h"
#include <algorithm>

// ============================================================================
// PUBLIC API
// ============================================================================

std::pair<double,double> GimbalStabilizer::computeStabilizedVelocity(
    double desiredAzVel_dps,
    double desiredElVel_dps,
    double imuRoll_deg,
    double imuPitch_deg,
    double imuYaw_deg,
    double gyroX_dps,
    double gyroY_dps,
    double gyroZ_dps,
    double currentAz_deg,
    double currentEl_deg,
    double targetAz_world,
    double targetEl_world,
    bool useWorldTarget,
    double dt) const
{
    // ========================================================================
    // Control Law: ω_cmd = ω_user + ω_feedforward + Kp × (angle_error)
    // ========================================================================

    double finalAz_dps = desiredAzVel_dps;
    double finalEl_dps = desiredElVel_dps;

    // If world-frame target holding is disabled, return user velocity only
    if (!useWorldTarget) {
        return {finalAz_dps, finalEl_dps};
    }

    // ========================================================================
    // COMPONENT 1: Position Correction (AHRS-based drift compensation)
    // ========================================================================

    // Access runtime-configurable tuning parameters
    const auto& cfg = MotionTuningConfig::instance().stabilizer;

    // Compute required gimbal angles to point at world target
    auto [requiredAz_deg, requiredEl_deg] = computeRequiredGimbalAngles(
        imuRoll_deg, imuPitch_deg, imuYaw_deg,
        targetAz_world, targetEl_world
    );

    // Calculate angle errors (required - current)
    double azError_deg = normalizeAngle180(requiredAz_deg - currentAz_deg);
    double elError_deg = normalizeAngle180(requiredEl_deg - currentEl_deg);

    // Convert to velocity correction: v_correction = Kp × error
    double azPositionCorr_dps = cfg.kpPosition * azError_deg;
    double elPositionCorr_dps = cfg.kpPosition * elError_deg;

    // Clamp position correction velocities
    azPositionCorr_dps = std::clamp(azPositionCorr_dps, -cfg.maxPositionVel, cfg.maxPositionVel);
    elPositionCorr_dps = std::clamp(elPositionCorr_dps, -cfg.maxPositionVel, cfg.maxPositionVel);

    // ========================================================================
    // COMPONENT 2: Rate Feed-Forward (Gyro-based transient compensation)
    // ========================================================================

    // IMU body rates: X→p (roll), Y→q (pitch), Z→r (yaw, inverted)
    double p_dps = gyroX_dps;
    double q_dps = gyroY_dps;
    double r_dps = -gyroZ_dps;  // Negate because IMU Z is DOWN

    // Transform platform motion to gimbal frame
    auto [azRateFF_dps, elRateFF_dps] = computeRateFeedForward(
        p_dps, q_dps, r_dps,
        currentAz_deg, currentEl_deg
    );

    // Clamp rate feed-forward velocities
    azRateFF_dps = std::clamp(azRateFF_dps, -cfg.maxVelocityCorr, cfg.maxVelocityCorr);
    elRateFF_dps = std::clamp(elRateFF_dps, -cfg.maxVelocityCorr, cfg.maxVelocityCorr);

    // ========================================================================
    // COMPONENT 3: Velocity Composition
    // ========================================================================

    // Combine all three components: user + position correction + rate feed-forward
    finalAz_dps = desiredAzVel_dps + azPositionCorr_dps + azRateFF_dps;
    finalEl_dps = desiredElVel_dps + elPositionCorr_dps + elRateFF_dps;

    // Apply total velocity limit
    finalAz_dps = std::clamp(finalAz_dps, -cfg.maxTotalVel, cfg.maxTotalVel);
    finalEl_dps = std::clamp(finalEl_dps, -cfg.maxTotalVel, cfg.maxTotalVel);

    return {finalAz_dps, finalEl_dps};
}

// ============================================================================
// PRIVATE IMPLEMENTATION
// ============================================================================

std::pair<double,double> GimbalStabilizer::computeRequiredGimbalAngles(
    double roll_deg,
    double pitch_deg,
    double yaw_deg,
    double targetAz_world,
    double targetEl_world) const
{
    // ========================================================================
    // Matrix-based rotation approach (avoids gimbal lock and sign errors)
    // ========================================================================

    // Convert to radians
    double roll = degToRad(roll_deg);
    double pitch = degToRad(pitch_deg);
    double yaw = degToRad(yaw_deg);
    double target_az = degToRad(targetAz_world);
    double target_el = degToRad(targetEl_world);

    // Step 1: Build platform rotation matrix (world → platform)
    // Rplat = Rz(yaw) * Ry(pitch) * Rx(roll)

    Eigen::Matrix3d Rz, Ry, Rx;

    // Rotation about Z-axis (yaw)
    Rz <<  std::cos(yaw), -std::sin(yaw), 0,
           std::sin(yaw),  std::cos(yaw), 0,
                0,              0,         1;

    // Rotation about Y-axis (pitch)
    Ry <<  std::cos(pitch), 0, std::sin(pitch),
                0,          1,      0,
          -std::sin(pitch), 0, std::cos(pitch);

    // Rotation about X-axis (roll)
    Rx << 1,      0,              0,
          0, std::cos(roll), -std::sin(roll),
          0, std::sin(roll),  std::cos(roll);

    Eigen::Matrix3d Rplat = Rz * Ry * Rx;

    // Step 2: Create unit vector pointing at world target
    double cos_el = std::cos(target_el);
    Eigen::Vector3d v_world;
    v_world << cos_el * std::cos(target_az),
               cos_el * std::sin(target_az),
               std::sin(target_el);

    // Step 3: Transform target vector to platform frame
    // v_platform = Rplat^T * v_world
    Eigen::Vector3d v_platform = Rplat.transpose() * v_world;

    // Step 4: Extract gimbal angles from platform-frame vector
    double gimbalAz_rad = std::atan2(v_platform.y(), v_platform.x());
    double gimbalEl_rad = std::atan2(v_platform.z(),
                                      std::sqrt(v_platform.x() * v_platform.x() +
                                                v_platform.y() * v_platform.y()));

    // Convert to degrees
    double gimbalAz_deg = radToDeg(gimbalAz_rad);
    double gimbalEl_deg = radToDeg(gimbalEl_rad);

    // ✅ EXPERT REVIEW FIX: Negate elevation to match system sign convention
    // Elevation servo wiring convention requires sign inversion
    gimbalEl_deg = -gimbalEl_deg;

    return {gimbalAz_deg, gimbalEl_deg};
}

std::pair<double,double> GimbalStabilizer::computeRateFeedForward(
    double p_dps,
    double q_dps,
    double r_dps,
    double gimbalAz_deg,
    double gimbalEl_deg) const
{
    // ========================================================================
    // Kinematic coupling: Transform platform angular rates to gimbal frame
    // ========================================================================

    // Convert gimbal angles to radians
    double az_rad = degToRad(gimbalAz_deg);
    double el_rad = degToRad(gimbalEl_deg);

    double sin_az = std::sin(az_rad);
    double cos_az = std::cos(az_rad);

    // ✅ SAFETY: Clamp tan(el) to prevent singularities near ±90° elevation
    const auto& cfg = MotionTuningConfig::instance().stabilizer;
    double tan_el = std::tan(el_rad);
    tan_el = std::clamp(tan_el, -cfg.maxTanEl, cfg.maxTanEl);

    // Platform motion effect on elevation axis
    // Elevation couples with platform pitch and roll (depending on azimuth orientation)
    double platformEffectOnEl_dps = q_dps * cos_az - p_dps * sin_az;

    // Platform motion effect on azimuth axis
    // Azimuth couples with platform yaw + pitch/roll projected through elevation angle
    double platformEffectOnAz_dps = r_dps + tan_el * (q_dps * sin_az + p_dps * cos_az);

    // Return negative (feed-forward compensates platform motion)
    return {-platformEffectOnAz_dps, -platformEffectOnEl_dps};
}
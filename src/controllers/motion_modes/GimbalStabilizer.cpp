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
    //double azPositionCorr_dps = cfg.kpPosition * azError_deg;
    //double elPositionCorr_dps = cfg.kpPosition * elError_deg;

    // ✅ NEW: Derivative term for damping (error rate estimation)
    double azErrorRate_dps = (azError_deg - m_prevAzError_deg) / dt;
    double elErrorRate_dps = (elError_deg - m_prevElError_deg) / dt;
    m_prevAzError_deg = azError_deg;
    m_prevElError_deg = elError_deg;

    // ✅ NEW: Filter derivative to reduce noise (optional but recommended)
    azErrorRate_dps = std::clamp(azErrorRate_dps, -cfg.maxErrorRate, cfg.maxErrorRate);
    elErrorRate_dps = std::clamp(elErrorRate_dps, -cfg.maxErrorRate, cfg.maxErrorRate);

    // Convert to velocity correction: PD control
    // v_correction = Kp × error + Kd × error_rate
    double azPositionCorr_dps = cfg.kpPosition * azError_deg + cfg.kdPosition * azErrorRate_dps;
    double elPositionCorr_dps = cfg.kpPosition * elError_deg + cfg.kdPosition * elErrorRate_dps;

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
// DEBUG VERSION - Populates debug struct for OSD visualization
// ============================================================================

std::pair<double,double> GimbalStabilizer::computeStabilizedVelocityWithDebug(
    SystemStateData::StabilizationDebug& dbg,
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

    // Store user input for debug
    dbg.userAz_dps = desiredAzVel_dps;
    dbg.userEl_dps = desiredElVel_dps;
    dbg.worldTargetHeld = useWorldTarget;

    double finalAz_dps = desiredAzVel_dps;
    double finalEl_dps = desiredElVel_dps;

    // IMU body rates: X→p (roll), Y→q (pitch), Z→r (yaw, inverted)
    dbg.p_dps = gyroX_dps;
    dbg.q_dps = gyroY_dps;
    dbg.r_dps = -gyroZ_dps;  // NOTE: Sign inversion for NED convention

    // If world-frame target holding is disabled, return user velocity only
    if (!useWorldTarget) {
        dbg.stabActive = false;
        dbg.requiredAz_deg = 0.0;
        dbg.requiredEl_deg = 0.0;
        dbg.azError_deg = 0.0;
        dbg.elError_deg = 0.0;
        dbg.azPosCorr_dps = 0.0;
        dbg.elPosCorr_dps = 0.0;
        dbg.azRateFF_dps = 0.0;
        dbg.elRateFF_dps = 0.0;
        dbg.finalAz_dps = finalAz_dps;
        dbg.finalEl_dps = finalEl_dps;
        return {finalAz_dps, finalEl_dps};
    }

    dbg.stabActive = true;

    // ========================================================================
    // COMPONENT 1: Position Correction (AHRS-based drift compensation)
    // ========================================================================

    const auto& cfg = MotionTuningConfig::instance().stabilizer;

        // ✅ FIX 1: Filter AHRS angles to reduce noise-induced jitter
    if (!m_ahrsFilterInitialized) {
        m_filteredRoll_deg = imuRoll_deg;
        m_filteredPitch_deg = imuPitch_deg;
        m_filteredYaw_deg = imuYaw_deg;
        m_ahrsFilterInitialized = true;
    } else {
        double alpha = 1.0 - std::exp(-dt / cfg.ahrsFilterTau);
        m_filteredRoll_deg  += alpha * (imuRoll_deg - m_filteredRoll_deg);
        m_filteredPitch_deg += alpha * (imuPitch_deg - m_filteredPitch_deg);
        
        // ✅ Special handling for yaw wraparound
        double yawDiff = normalizeAngle180(imuYaw_deg - m_filteredYaw_deg);
        m_filteredYaw_deg = normalizeAngle180(m_filteredYaw_deg + alpha * yawDiff);
    }

    // Compute required gimbal angles to point at world target
    auto [requiredAz_deg, requiredEl_deg] = computeRequiredGimbalAngles(
        m_filteredRoll_deg, m_filteredPitch_deg, m_filteredYaw_deg,  // ✅ FILTERED!
        targetAz_world, targetEl_world
    );
    dbg.requiredAz_deg = requiredAz_deg;
    dbg.requiredEl_deg = requiredEl_deg;

    // Calculate angle errors (required - current)
    double azError_deg = normalizeAngle180(requiredAz_deg - currentAz_deg);
    double elError_deg = normalizeAngle180(requiredEl_deg - currentEl_deg);

    // ✅ FIX: Calculate derivative on RAW error BEFORE deadband
    double azErrorRate_dps = 0.0;
    double elErrorRate_dps = 0.0;
    if (dt > 1e-6) {
        azErrorRate_dps = (azError_deg - m_prevAzError_deg) / dt;
        elErrorRate_dps = (elError_deg - m_prevElError_deg) / dt;
        
        // Clamp derivative to prevent spikes
        azErrorRate_dps = std::clamp(azErrorRate_dps, -cfg.maxErrorRate, cfg.maxErrorRate);
        elErrorRate_dps = std::clamp(elErrorRate_dps, -cfg.maxErrorRate, cfg.maxErrorRate);
    }
    // ✅ Store RAW error for next derivative calculation
    m_prevAzError_deg = azError_deg;
    m_prevElError_deg = elError_deg;

    // ✅ Apply deadband ONLY to proportional term (after derivative calc)
    double azErrorForP = azError_deg;
    double elErrorForP = elError_deg;
    if (std::abs(azErrorForP) < cfg.positionDeadbandDeg) azErrorForP = 0.0;
    if (std::abs(elErrorForP) < cfg.positionDeadbandDeg) elErrorForP = 0.0;

    // ✅ Also apply deadband to derivative when error is small (prevents chatter)
    if (std::abs(azError_deg) < cfg.positionDeadbandDeg) azErrorRate_dps = 0.0;
    if (std::abs(elError_deg) < cfg.positionDeadbandDeg) elErrorRate_dps = 0.0;

    dbg.azError_deg = azErrorForP;
    dbg.elError_deg = elErrorForP;

    // PD Control with corrected terms
    double azPositionCorr_dps = cfg.kpPosition * azErrorForP + cfg.kdPosition * azErrorRate_dps;
    double elPositionCorr_dps = cfg.kpPosition * elErrorForP + cfg.kdPosition * elErrorRate_dps;
    dbg.azPosCorr_dps = azPositionCorr_dps;
    dbg.elPosCorr_dps = elPositionCorr_dps;

 // ========================================================================
// COMPONENT 2: Rate Feed-Forward (Gyro-based, SMOOTH & SAFE)
// ========================================================================

auto [azRateFF_raw, elRateFF_raw] = computeRateFeedForward(
    dbg.p_dps, dbg.q_dps, dbg.r_dps,
    currentAz_deg, currentEl_deg
);

// Clamp raw FF
azRateFF_raw = std::clamp(azRateFF_raw, -cfg.maxVelocityCorr, cfg.maxVelocityCorr);
elRateFF_raw = std::clamp(elRateFF_raw, -cfg.maxVelocityCorr, cfg.maxVelocityCorr);

// ------------------------------------------------------------------
// Smooth gyro-based FF ramp (NO DISCONTINUITY)
// ------------------------------------------------------------------
constexpr double GYRO_DB = 0.3;     // deg/s (noise)
constexpr double GYRO_FULL = 1.2;   // deg/s (full FF)

double yawRate = std::abs(dbg.r_dps);
double elRate  = std::hypot(dbg.p_dps, dbg.q_dps);

double azFFScale = std::clamp((yawRate - GYRO_DB) / (GYRO_FULL - GYRO_DB), 0.0, 1.0);
double elFFScale = std::clamp((elRate  - GYRO_DB) / (GYRO_FULL - GYRO_DB), 0.0, 1.0);

// Apply scaling
azRateFF_raw *= azFFScale;
elRateFF_raw *= elFFScale;

// ------------------------------------------------------------------
// FF smoothing (models actuator inertia, kills jitter)
// ------------------------------------------------------------------
double alphaFF = 1.0 - std::exp(-dt / 0.10); // τ = 100 ms

m_azFF_smooth += alphaFF * (azRateFF_raw - m_azFF_smooth);
m_elFF_smooth += alphaFF * (elRateFF_raw - m_elFF_smooth);

dbg.azRateFF_dps = m_azFF_smooth;
dbg.elRateFF_dps = m_elFF_smooth;


// ========================================================================
// COMPONENT 3: Velocity Composition (ACCEL-LIMITED)
// ========================================================================

double azCmd = desiredAzVel_dps + azPositionCorr_dps + m_azFF_smooth;
double elCmd = desiredElVel_dps + elPositionCorr_dps + m_elFF_smooth;

// Total velocity limit
azCmd = std::clamp(azCmd, -cfg.maxTotalVel, cfg.maxTotalVel);
elCmd = std::clamp(elCmd, -cfg.maxTotalVel, cfg.maxTotalVel);

// ------------------------------------------------------------------
// Acceleration limiting (CRITICAL for smoothness)
// ------------------------------------------------------------------
constexpr double MAX_ACC_DPS2 = 35.0;   // safe for EO heads

double maxDelta = MAX_ACC_DPS2 * dt;

azCmd = std::clamp(azCmd,
                   m_prevAzCmd_dps - maxDelta,
                   m_prevAzCmd_dps + maxDelta);

elCmd = std::clamp(elCmd,
                   m_prevElCmd_dps - maxDelta,
                   m_prevElCmd_dps + maxDelta);

// Store
m_prevAzCmd_dps = azCmd;
m_prevElCmd_dps = elCmd;

dbg.finalAz_dps = azCmd;
dbg.finalEl_dps = elCmd;

return { azCmd, elCmd };
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
    //gimbalEl_deg = -gimbalEl_deg;

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

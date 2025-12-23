#ifndef GIMBALSTABILIZER_H
#define GIMBALSTABILIZER_H

#include <Eigen/Dense>
#include <utility>
#include <cmath>
#include "models/domain/systemstatedata.h"  // For StabilizationDebug struct

/**
 * @class GimbalStabilizer
 * @brief Platform stabilization for azimuth-over-elevation gimbal systems
 *
 * Implements industry-standard stabilization for military turret control systems
 * (used in CROWS, EOS R400, FLIR turrets, etc.)
 *
 * Control Law:
 *   ω_cmd = ω_user + ω_feedforward + Kp × (angle_error)
 *
 * Where:
 *   ω_user         = User-commanded velocity (joystick/tracking/scan)
 *   ω_feedforward  = Gyro-based transient platform motion compensation
 *   Kp × error     = AHRS-based position correction (drift compensation)
 *
 * Mathematical Approach:
 *   - Matrix-based rotation (Rplat = Rz*Ry*Rx) for gimbal lock avoidance
 *   - Proper kinematic transformation for rate feed-forward
 *   - Velocity-domain composition (matches AZD-KD velocity control)
 */
class GimbalStabilizer
{
public:
    GimbalStabilizer() = default;

    /**
     * @brief Compute stabilized velocity commands for gimbal servos
     *
     * @param desiredAzVel_dps User-commanded azimuth velocity (deg/s)
     * @param desiredElVel_dps User-commanded elevation velocity (deg/s)
     * @param imuRoll_deg Platform roll angle from AHRS (deg)
     * @param imuPitch_deg Platform pitch angle from AHRS (deg)
     * @param imuYaw_deg Platform yaw angle from AHRS (deg)
     * @param gyroX_dps Filtered IMU gyro X (roll rate, deg/s)
     * @param gyroY_dps Filtered IMU gyro Y (pitch rate, deg/s)
     * @param gyroZ_dps Filtered IMU gyro Z (yaw rate, deg/s)
     * @param currentAz_deg Current gimbal azimuth (deg)
     * @param currentEl_deg Current gimbal elevation (deg)
     * @param targetAz_world Target world azimuth (deg, 0=North)
     * @param targetEl_world Target world elevation (deg, 0=horizon)
     * @param useWorldTarget Enable world-frame target holding
     * @param dt Time delta since last update (seconds)
     *
     * @return std::pair<double,double> Stabilized velocity commands (az_dps, el_dps)
     */
    std::pair<double,double> computeStabilizedVelocity(
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
        double dt) const;

    /**
     * @brief Compute stabilized velocity with debug output for OSD visualization
     *
     * Same as computeStabilizedVelocity but also populates debug struct
     * for diagnostics display on OSD overlay.
     *
     * @param debugOut Output debug data for OSD display
     * @param ... (same parameters as above)
     */
    std::pair<double,double> computeStabilizedVelocityWithDebug(
        SystemStateData::StabilizationDebug& debugOut,
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
        double dt) const;

private:
    // ========================================================================
    // Internal computation methods
    // ========================================================================

    /**
     * @brief Compute required gimbal angles for world-frame target using matrices
     *
     * Uses proper rotation matrix approach: Rplat = Rz*Ry*Rx
     * Then transforms world target vector to platform frame: v_plat = Rplat^T * v_world
     *
     * @param roll_deg Platform roll (deg)
     * @param pitch_deg Platform pitch (deg)
     * @param yaw_deg Platform yaw (deg)
     * @param targetAz_world World azimuth (deg)
     * @param targetEl_world World elevation (deg)
     * @return std::pair<double,double> Required gimbal angles (az_deg, el_deg)
     */
    std::pair<double,double> computeRequiredGimbalAngles(
        double roll_deg,
        double pitch_deg,
        double yaw_deg,
        double targetAz_world,
        double targetEl_world) const;

    /**
     * @brief Compute velocity feed-forward from platform angular rates
     *
     * Transforms IMU gyro rates (p,q,r) to gimbal frame using kinematic coupling.
     * Compensates for transient platform motion (pitch/roll/yaw rates).
     *
     * @param p_dps Platform roll rate (deg/s)
     * @param q_dps Platform pitch rate (deg/s)
     * @param r_dps Platform yaw rate (deg/s)
     * @param gimbalAz_deg Current gimbal azimuth (deg)
     * @param gimbalEl_deg Current gimbal elevation (deg)
     * @return std::pair<double,double> Rate feed-forward velocities (az_dps, el_dps)
     */
    std::pair<double,double> computeRateFeedForward(
        double p_dps,
        double q_dps,
        double r_dps,
        double gimbalAz_deg,
        double gimbalEl_deg) const;

    // ========================================================================
    // Tuning parameters
    // ========================================================================
    // All tuning parameters are now loaded from motion_tuning.json at runtime
    // via MotionTuningConfig::instance().stabilizer
    //
    // Configurable parameters:
    //   - kpPosition:       Position error gain (deg/s per deg) [default: 2.0]
    //   - maxPositionVel:   Max position correction velocity (deg/s) [default: 10.0]
    //   - maxVelocityCorr:  Max rate feed-forward velocity (deg/s) [default: 5.0]
    //   - maxTotalVel:      Max total correction velocity (deg/s) [default: 12.0]
    //   - maxTanEl:         Clamp tan(elevation) for singularity protection [default: 10.0]

    // ========================================================================
    // Helper functions
    // ========================================================================

    inline double degToRad(double d) const { return d * M_PI / 180.0; }
    inline double radToDeg(double r) const { return r * 180.0 / M_PI; }

    inline double normalizeAngle180(double angle) const {
        while (angle > 180.0) angle -= 360.0;
        while (angle < -180.0) angle += 360.0;
        return angle;
    }

    mutable double m_prevAzError_deg = 0.0;
    mutable double m_prevElError_deg = 0.0;

    // ✅ NEW: AHRS angle filters (reduce yaw noise)
    mutable double m_filteredYaw_deg = 0.0;
    mutable double m_filteredPitch_deg = 0.0;
    mutable double m_filteredRoll_deg = 0.0;
    mutable bool m_ahrsFilterInitialized = false;
    mutable bool  m_requiredAnglesInitialized = false;
    mutable double  m_filteredRequiredAz_deg = 0.0;
    mutable double  m_filteredRequiredEl_deg = 0.0;

mutable double m_prevAzCmd_dps = 0.0;
mutable double m_prevElCmd_dps = 0.0;
mutable double m_azFF_smooth = 0.0;
mutable double m_elFF_smooth = 0.0;



};

#endif // GIMBALSTABILIZER_H

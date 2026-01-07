#ifndef MOTIONTUNINGCONFIG_H
#define MOTIONTUNINGCONFIG_H

#include <QString>

/**
 * @brief Motion Control Tuning Configuration
 *
 * Loads runtime-configurable motion control parameters from motion_tuning.json.
 * This allows field tuning of PID gains, filters, and motion limits without rebuilding.
 *
 * Architecture follows DeviceConfiguration pattern for consistency.
 *
 * Usage:
 *   const auto& cfg = MotionTuningConfig::instance();
 *   m_azPid.Kp = cfg.trackingAz.kp;
 */
class MotionTuningConfig
{
public:
    /**
     * @brief PID controller gains structure
     */
    struct PIDGains {
        double kp = 1.0;           ///< Proportional gain
        double ki = 0.01;          ///< Integral gain
        double kd = 0.05;          ///< Derivative gain
        double maxIntegral = 20.0; ///< Integral windup limit
    };

    /**
     * @brief Filter configuration structure
     */
    struct FilterConfig {
        double gyroCutoffFreqHz = 5.0;        ///< Gyro low-pass filter cutoff (Hz)
        double trackingPositionTau = 0.12;    ///< Tracking position smoothing time constant (s)
        double trackingVelocityTau = 0.08;    ///< Tracking velocity smoothing time constant (s)
        double manualJoystickTau = 0.08;      ///< Manual mode joystick filter time constant (s)
    };

    /**
     * @brief Motion limits configuration structure
     */
    struct MotionLimits {
        double maxAccelerationDegS2 = 50.0;   ///< General max acceleration (deg/s²)
        double scanMaxAccelDegS2 = 20.0;      ///< AutoSectorScan acceleration (deg/s²)
        double trpMaxAccelDegS2 = 50.0;       ///< TRP scan acceleration (deg/s²)
        double trpDefaultTravelSpeed = 15.0;  ///< TRP travel speed (deg/s)
        double maxVelocityDegS = 30.0;        ///< General velocity limit (deg/s)
        double arrivalThresholdDeg = 0.5;     ///< Distance to consider target "reached" (deg)
        double updateIntervalS = 0.05;        ///< Control loop update interval (s)
    };

    /**
     * @brief Servo conversion constants structure
     */
    struct ServoConstants {
        double azStepsPerDegree = 618.0556;   ///< Azimuth servo steps/degree (222500/360)
        double elStepsPerDegree = 555.5556;   ///< Elevation servo steps/degree (200000/360)
    };

    /**
     * @brief Axis-specific AZD-KX servo parameters
     *
     * CRITICAL: These parameters are optimized for RCWS application:
     * - Azimuth: Heavy load (turret), needs smooth decel to avoid overvoltage
     * - Elevation: Lighter load, needs crisp decel to avoid overshoot
     *
     * Overvoltage prevention (Oriental Motor datasheet):
     * - Large inertial load + sudden stop = regenerative energy spike
     * - Slower decel rate dissipates energy over longer time
     *
     * Registers written in single Modbus transaction:
     * 0x005E-0x005F: Speed (2 regs)
     * 0x0060-0x0061: Accel (2 regs)
     * 0x0062-0x0063: Decel (2 regs)
     * 0x0064-0x0065: Current limit (2 regs) - 1000 = 100%
     * 0x0066-0x0067: Trigger (2 regs)
     */
    struct AxisServoParams {
        quint32 accelHz = 150000;      ///< Acceleration rate (Hz/s)
        quint32 decelHz = 150000;      ///< Deceleration rate (Hz/s)
        quint32 currentPercent = 1000; ///< Current limit (1000 = 100%, 700 = 70%)
        double maxSpeedScale = 1.0;    ///< Speed multiplier (1.0 = 100%, 0.7 = 70%)
    };

    /**
     * @brief Combined axis parameters
     */
    struct AxisServoConfig {
        AxisServoParams azimuth;   ///< Azimuth axis (heavy turret load)
        AxisServoParams elevation; ///< Elevation axis (lighter gun load)
    };

    /**
     * @brief Mode-specific scan parameters
     */
    struct ScanParams {
        double decelerationDistanceDeg = 5.0; ///< Distance to start deceleration (deg)
        double arrivalThresholdDeg = 0.2;     ///< Distance to consider arrived (deg)
    };

    /**
     * @brief Manual mode acceleration limits
     */
    struct ManualLimits {
        double maxAccelHzPerSec = 500000.0;   ///< Max acceleration in Hz/s
    };

    /**
     * @brief Stabilizer tuning parameters (GimbalStabilizer)
     *
     * IMPORTANT: Limit interplay and constraints:
     * - maxTotalVel should be >= (maxPositionVel + maxVelocityCorr) to avoid saturation
     * - Recommended: maxTotalVel = maxPositionVel + maxVelocityCorr + margin
     * - If maxTotalVel < sum of components, position correction may be clipped
     * - maxTanEl prevents singularities near ±90° elevation (tan(84.3°) ≈ 10)
     *
     * Defaults tuned for 50ms update rate (20Hz) on moving platform.
     */
struct StabilizerConfig {
    double kpPosition = 0.7;
    double kdPosition = 0.0;
    double maxErrorRate = 50.0;
    double maxPositionVel = 10.0;
    double maxVelocityCorr = 5.0;
    double maxTotalVel = 18.0;
    double maxTanEl = 10.0;

    // ✅ NEW: Anti-jitter parameters
    double positionDeadbandDeg = 0.5;   ///< Ignore errors smaller than this
    double ahrsFilterTau = 0.15;          ///< AHRS angle filter time constant (seconds)
};

    // ============================================================================
    // PUBLIC API
    // ============================================================================

    /**
     * @brief Load configuration from JSON file
     * @param path Path to motion_tuning.json (default: ./config/motion_tuning.json)
     * @return true if loaded successfully, false otherwise
     */
    static bool load(const QString& path = "./config/motion_tuning.json");

    /**
     * @brief Get singleton instance
     * @return Reference to the loaded configuration
     */
    static const MotionTuningConfig& instance();

    /**
     * @brief Check if configuration has been successfully loaded
     * @return true if load() was called and succeeded
     */
    static bool isLoaded();

    // ============================================================================
    // CONFIGURATION ACCESSORS
    // ============================================================================

    // Filter parameters
    FilterConfig filters;

    // Motion limits
    MotionLimits motion;

    // Servo constants
    ServoConstants servo;

    // PID gains per mode
    PIDGains trackingAz;
    PIDGains trackingEl;
    PIDGains autoScanAz;
    PIDGains autoScanEl;
    PIDGains trpScanAz;
    PIDGains trpScanEl;
    PIDGains radarSlewAz;
    PIDGains radarSlewEl;

    // Mode-specific parameters
    ScanParams autoScanParams;
    ScanParams trpScanParams;
    ManualLimits manualLimits;
    StabilizerConfig stabilizer;

    // Axis-specific servo parameters (AZD-KX optimization)
    AxisServoConfig axisServo;

private:
    /**
     * @brief Private constructor (singleton pattern)
     */
    MotionTuningConfig() = default;

    /**
     * @brief Load configuration from specified file
     * @param filePath Full path to JSON file
     * @return true if successful
     */
    static bool loadFromFile(const QString& filePath);

    /**
     * @brief Parse PID gains from JSON object
     */
    static PIDGains parsePIDGains(const class QJsonObject& obj);

    // Singleton instance
    static MotionTuningConfig m_instance;
    static bool m_loaded;
};

#endif // MOTIONTUNINGCONFIG_H

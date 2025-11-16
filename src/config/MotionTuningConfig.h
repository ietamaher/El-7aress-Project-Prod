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

    // ========================================================================
    // PUBLIC API
    // ========================================================================

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

    // ========================================================================
    // CONFIGURATION ACCESSORS
    // ========================================================================

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

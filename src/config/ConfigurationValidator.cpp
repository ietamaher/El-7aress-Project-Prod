#include "ConfigurationValidator.h"
#include "controllers/deviceconfiguration.h"
#include "MotionTuningConfig.h"
#include "AppConstants.h"

#include <QFile>
#include <QDebug>

using namespace RcwsConstants;

// Initialize static members
QStringList ConfigurationValidator::m_errors;
QStringList ConfigurationValidator::m_warnings;

bool ConfigurationValidator::validateAll()
{
    clearMessages();
    qInfo() << "=== Validating Configuration ===";

    bool valid = true;

    valid &= validateSystem();
    valid &= validateVideo();
    valid &= validateGimbal();
    valid &= validateBallistics();
    valid &= validateUI();
    valid &= validateSafety();
    valid &= validatePerformance();
    valid &= validateHardware();
    valid &= validateMotionTuning();

    if (!m_errors.isEmpty()) {
        qCritical() << "Configuration validation FAILED with" << m_errors.count() << "errors:";
        for (const QString& error : m_errors) {
            qCritical() << "  ✗" << error;
        }
    }

    if (!m_warnings.isEmpty()) {
        qWarning() << "Configuration has" << m_warnings.count() << "warnings:";
        for (const QString& warning : m_warnings) {
            qWarning() << "  ⚠" << warning;
        }
    }

    if (m_errors.isEmpty() && m_warnings.isEmpty()) {
        qInfo() << "  ✓ Configuration validation PASSED";
    }

    return valid && m_errors.isEmpty();
}

bool ConfigurationValidator::validateSystem()
{
    const auto& cfg = DeviceConfiguration::system();
    bool valid = true;

    // Validate system name
    if (cfg.name.isEmpty()) {
        addError("System name cannot be empty");
        valid = false;
    }

    // Validate version format
    if (cfg.version.isEmpty()) {
        addError("System version cannot be empty");
        valid = false;
    }

    // Validate accent color format
    if (!cfg.accentColor.startsWith("#") || cfg.accentColor.length() != 7) {
        addError("Accent color must be in #RRGGBB format");
        valid = false;
    }

    // Validate log level
    QStringList validLogLevels = {"debug", "info", "warning", "error", "critical"};
    if (!validLogLevels.contains(cfg.logLevel.toLower())) {
        addWarning(QString("Invalid log level '%1', will use 'info'").arg(cfg.logLevel));
    }

    return valid;
}

bool ConfigurationValidator::validateVideo()
{
    const auto& cfg = DeviceConfiguration::video();
    bool valid = true;

    // Validate video dimensions
    valid &= validateRange(cfg.sourceWidth, Video::MIN_VIDEO_WIDTH, Video::MAX_VIDEO_WIDTH, "Video width");
    valid &= validateRange(cfg.sourceHeight, Video::MIN_VIDEO_HEIGHT, Video::MAX_VIDEO_HEIGHT, "Video height");

    // Validate device paths
    if (cfg.dayDevicePath.isEmpty()) {
        addError("Day camera device path cannot be empty");
        valid = false;
    }

    if (cfg.nightDevicePath.isEmpty()) {
        addError("Night camera device path cannot be empty");
        valid = false;
    }

    // Check if video devices exist (warning only, as devices may not be connected during config load)
    if (!QFile::exists(cfg.dayDevicePath)) {
        addWarning(QString("Day camera device not found: %1").arg(cfg.dayDevicePath));
    }

    if (!QFile::exists(cfg.nightDevicePath)) {
        addWarning(QString("Night camera device not found: %1").arg(cfg.nightDevicePath));
    }

    return valid;
}

bool ConfigurationValidator::validateGimbal()
{
    const auto& cfg = DeviceConfiguration::gimbal();
    bool valid = true;

    // Validate azimuth limits
    if (cfg.azimuthMin >= cfg.azimuthMax) {
        addError("Gimbal azimuth min must be less than max");
        valid = false;
    }

    if (cfg.azimuthMin < -360.0f || cfg.azimuthMax > 360.0f) {
        addError("Gimbal azimuth limits must be within [-360, 360] degrees");
        valid = false;
    }

    // Validate elevation limits
    if (cfg.elevationMin >= cfg.elevationMax) {
        addError("Gimbal elevation min must be less than max");
        valid = false;
    }

    if (cfg.elevationMin < -90.0f || cfg.elevationMax > 90.0f) {
        addError("Gimbal elevation limits must be within [-90, 90] degrees");
        valid = false;
    }

    // Validate speeds
    valid &= validateRange(cfg.maxSlewSpeed, Gimbal::MIN_SLEW_SPEED, Gimbal::MAX_SLEW_SPEED, "Max slew speed");
    valid &= validateRange(cfg.defaultSlewSpeed, Gimbal::MIN_SLEW_SPEED, cfg.maxSlewSpeed, "Default slew speed");

    // Validate acceleration
    valid &= validateRange(cfg.acceleration, 1.0f, Gimbal::MAX_ACCELERATION, "Gimbal acceleration");

    // Validate dead zone
    valid &= validateRange(cfg.joystickDeadZone, 0.0f, 0.5f, "Joystick dead zone");

    return valid;
}

bool ConfigurationValidator::validateBallistics()
{
    const auto& cfg = DeviceConfiguration::ballistics();
    bool valid = true;

    // Validate zeroing limits
    valid &= validateRange(cfg.maxZeroingOffset, 0.0f, Ballistics::MAX_ZEROING_AZIMUTH_OFFSET, "Max zeroing offset");
    valid &= validateRange(cfg.zeroingStepSize, 0.01f, 1.0f, "Zeroing step size");

    // Validate windage
    valid &= validateRange(cfg.maxWindSpeed, 0.0f, Ballistics::MAX_WIND_SPEED, "Max wind speed");
    valid &= validateRange(cfg.windStepSize, 0.1f, 10.0f, "Wind step size");

    // Validate bullet speed
    valid &= validateRange(cfg.defaultBulletSpeed,
                          Ballistics::MIN_BULLET_SPEED,
                          Ballistics::MAX_BULLET_SPEED,
                          "Default bullet speed");

    return valid;
}

bool ConfigurationValidator::validateUI()
{
    const auto& cfg = DeviceConfiguration::ui();
    bool valid = true;

    // Validate OSD refresh rate
    valid &= validateRange(cfg.osdRefreshRate, 10, 60, "OSD refresh rate");

    // Validate font size
    valid &= validateRange(cfg.fontSize, Osd::MIN_FONT_SIZE, Osd::MAX_FONT_SIZE, "Font size");

    // Validate reticle type
    QStringList validReticles = {"BoxCrosshair", "BracketsReticle", "DuplexCrosshair", "FineCrosshair", "ChevronReticle"};
    if (!validReticles.contains(cfg.defaultReticle)) {
        addWarning(QString("Invalid default reticle '%1', will use 'BoxCrosshair'").arg(cfg.defaultReticle));
    }

    return valid;
}

bool ConfigurationValidator::validateSafety()
{
    const auto& cfg = DeviceConfiguration::safety();
    bool valid = true;

    // Validate temperature limits
    if (cfg.motorWarningTemp >= cfg.motorMaxTemp) {
        addError("Motor warning temp must be less than max temp");
        valid = false;
    }

    valid &= validateRange(cfg.motorMaxTemp, 50.0f, 120.0f, "Motor max temp");
    valid &= validateRange(cfg.motorWarningTemp, 40.0f, cfg.motorMaxTemp, "Motor warning temp");

    if (cfg.driverWarningTemp >= cfg.driverMaxTemp) {
        addError("Driver warning temp must be less than max temp");
        valid = false;
    }

    valid &= validateRange(cfg.driverMaxTemp, 50.0f, 120.0f, "Driver max temp");
    valid &= validateRange(cfg.driverWarningTemp, 40.0f, cfg.driverMaxTemp, "Driver warning temp");

    return valid;
}

bool ConfigurationValidator::validatePerformance()
{
    const auto& cfg = DeviceConfiguration::performance();
    bool valid = true;

    // Validate buffer sizes (must be positive and reasonable)
    valid &= validateRange(cfg.gimbalMotionBufferSize, 1000, 600000, "Gimbal motion buffer size");
    valid &= validateRange(cfg.imuDataBufferSize, 1000, 1000000, "IMU data buffer size");
    valid &= validateRange(cfg.trackingDataBufferSize, 1000, 360000, "Tracking data buffer size");
    valid &= validateRange(cfg.videoFrameBufferSize, 1, 100, "Video frame buffer size");

    return valid;
}

bool ConfigurationValidator::validateHardware()
{
    bool valid = true;

    // Validate IMU config
    const auto& imu = DeviceConfiguration::imu();
    if (imu.port.isEmpty()) {
        addError("IMU port cannot be empty");
        valid = false;
    }
    valid &= validateRange(imu.baudRate, 9600, 921600, "IMU baud rate");
    valid &= validateRange(imu.samplingRateHz, 25, 100, "IMU sampling rate (Hz)");

    // Validate LRF config
    const auto& lrf = DeviceConfiguration::lrf();
    if (lrf.port.isEmpty()) {
        addError("LRF port cannot be empty");
        valid = false;
    }
    valid &= validateRange(lrf.baudRate, 9600, 921600, "LRF baud rate");

    // Validate servo configs
    const auto& servoAz = DeviceConfiguration::servoAz();
    const auto& servoEl = DeviceConfiguration::servoEl();

    if (servoAz.port.isEmpty()) {
        addError("Servo azimuth port cannot be empty");
        valid = false;
    }
    if (servoEl.port.isEmpty()) {
        addError("Servo elevation port cannot be empty");
        valid = false;
    }

    valid &= validateRange(servoAz.baudRate, 9600, 921600, "Servo AZ baud rate");
    valid &= validateRange(servoEl.baudRate, 9600, 921600, "Servo EL baud rate");

    valid &= validateRange(servoAz.slaveId, 1, 247, "Servo AZ slave ID");
    valid &= validateRange(servoEl.slaveId, 1, 247, "Servo EL slave ID");

    // Validate PLC configs
    const auto& plc21 = DeviceConfiguration::plc21();
    const auto& plc42 = DeviceConfiguration::plc42();

    if (plc21.port.isEmpty()) {
        addError("PLC21 port cannot be empty");
        valid = false;
    }
    if (plc42.port.isEmpty()) {
        addError("PLC42 port cannot be empty");
        valid = false;
    }

    return valid;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void ConfigurationValidator::addError(const QString& message)
{
    m_errors.append(message);
}

void ConfigurationValidator::addWarning(const QString& message)
{
    m_warnings.append(message);
}

void ConfigurationValidator::clearMessages()
{
    m_errors.clear();
    m_warnings.clear();
}

bool ConfigurationValidator::validateRange(float value, float min, float max, const QString& fieldName)
{
    if (value < min || value > max) {
        addError(QString("%1 (%2) is out of range [%3, %4]")
                     .arg(fieldName)
                     .arg(value)
                     .arg(min)
                     .arg(max));
        return false;
    }
    return true;
}

bool ConfigurationValidator::validateRange(int value, int min, int max, const QString& fieldName)
{
    if (value < min || value > max) {
        addError(QString("%1 (%2) is out of range [%3, %4]")
                     .arg(fieldName)
                     .arg(value)
                     .arg(min)
                     .arg(max));
        return false;
    }
    return true;
}

bool ConfigurationValidator::validateFileExists(const QString& path, const QString& fieldName, bool required)
{
    if (!QFile::exists(path)) {
        if (required) {
            addError(QString("%1: File not found: %2").arg(fieldName, path));
            return false;
        } else {
            addWarning(QString("%1: File not found: %2").arg(fieldName, path));
        }
    }
    return true;
}

bool ConfigurationValidator::validatePortPath(const QString& port, const QString& fieldName)
{
    if (port.isEmpty()) {
        addError(QString("%1: Port path cannot be empty").arg(fieldName));
        return false;
    }

    // Check if it's a valid serial port path format
    if (!port.startsWith("/dev/")) {
        addWarning(QString("%1: Port path '%2' does not start with /dev/").arg(fieldName, port));
    }

    return true;
}

bool ConfigurationValidator::validateMotionTuning()
{
    // Skip validation if config wasn't loaded (will use defaults at runtime)
    if (!MotionTuningConfig::isLoaded()) {
        addWarning("Motion tuning config not loaded - using default values");
        return true; // Not a critical error, defaults will work
    }

    const auto& cfg = MotionTuningConfig::instance();
    bool valid = true;

    // Validate filter parameters
    valid &= validateRange(static_cast<float>(cfg.filters.gyroCutoffFreqHz), 1.0f, 20.0f,
                          "Gyro cutoff frequency");
    valid &= validateRange(static_cast<float>(cfg.filters.trackingPositionTau), 0.01f, 1.0f,
                          "Tracking position tau");
    valid &= validateRange(static_cast<float>(cfg.filters.trackingVelocityTau), 0.01f, 1.0f,
                          "Tracking velocity tau");
    valid &= validateRange(static_cast<float>(cfg.filters.manualJoystickTau), 0.01f, 0.5f,
                          "Manual joystick tau");

    // Validate motion limits
    valid &= validateRange(static_cast<float>(cfg.motion.maxAccelerationDegS2), 1.0f, 200.0f,
                          "Max acceleration");
    valid &= validateRange(static_cast<float>(cfg.motion.scanMaxAccelDegS2), 1.0f, 100.0f,
                          "Scan max acceleration");
    valid &= validateRange(static_cast<float>(cfg.motion.trpMaxAccelDegS2), 1.0f, 200.0f,
                          "TRP max acceleration");
    valid &= validateRange(static_cast<float>(cfg.motion.trpDefaultTravelSpeed), 1.0f, 120.0f,
                          "TRP default travel speed");
    valid &= validateRange(static_cast<float>(cfg.motion.maxVelocityDegS), 1.0f, 120.0f,
                          "Max velocity");
    valid &= validateRange(static_cast<float>(cfg.motion.arrivalThresholdDeg), 0.01f, 5.0f,
                          "Arrival threshold");

    // Validate PID gains (tracking mode)
    if (cfg.trackingAz.kp <= 0.0 || cfg.trackingAz.kp > 10.0) {
        addError("Tracking Az Kp must be in range (0, 10]");
        valid = false;
    }
    if (cfg.trackingAz.ki < 0.0 || cfg.trackingAz.ki > 1.0) {
        addError("Tracking Az Ki must be in range [0, 1]");
        valid = false;
    }
    if (cfg.trackingAz.kd < 0.0 || cfg.trackingAz.kd > 2.0) {
        addError("Tracking Az Kd must be in range [0, 2]");
        valid = false;
    }

    // Validate servo constants
    valid &= validateRange(static_cast<float>(cfg.servo.azStepsPerDegree), 100.0f, 10000.0f,
                          "Azimuth steps per degree");
    valid &= validateRange(static_cast<float>(cfg.servo.elStepsPerDegree), 100.0f, 10000.0f,
                          "Elevation steps per degree");

    // Validate manual limits
    valid &= validateRange(static_cast<float>(cfg.manualLimits.maxAccelHzPerSec), 10000.0f, 2000000.0f,
                          "Manual max acceleration Hz/s");

    return valid;
}

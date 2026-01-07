#ifndef DEVICECONFIGURATION_H
#define DEVICECONFIGURATION_H

#include <QObject>
#include <QString>
#include <QSerialPort>

class DeviceConfiguration {
public:
    struct VideoConfig {
        int sourceWidth = 1280;
        int sourceHeight = 720;
        QString dayDevicePath;
        QString dayControlPort;
        QString nightDevicePath;
        QString nightControlPort;
    };

    struct ImuConfig {
        QString port;
        int baudRate = 115200;
        int samplingRateHz = 100;            // 3DM-GX3-25 sampling rate (50-1000Hz)
        double tiltWarningThreshold = 30.0;  // Tilt warning threshold in degrees
        // Note: No slaveId - serial binary protocol, not Modbus
    };

    struct LrfConfig {
        QString port;
        int baudRate = 115200;
    };

    struct PlcConfig {
        QString port;
        int baudRate = 115200;
        int slaveId = 31;
        QSerialPort::Parity parity = QSerialPort::EvenParity;
    };

    struct ServoConfig {
        QString name;
        QString port;
        int baudRate = 230400;
        int slaveId = 1;
        QSerialPort::Parity parity = QSerialPort::NoParity;
    };

    struct ActuatorConfig {
        QString port;
        int baudRate = 115200;
    };

    struct SystemConfig {
        QString name = "El 7arress RCWS";
        QString version = "4.5";
        QString accentColor = "#46E2A5";
        QString logLevel = "info";
        QString logPath = "./logs/rcws.log";
        bool enableDataLogger = true;
        QString databasePath = "./data/rcws_history.db";
    };

    struct GimbalConfig {
        float azimuthMin = -180.0f;
        float azimuthMax = 180.0f;
        float elevationMin = -20.0f;
        float elevationMax = 60.0f;
        float maxSlewSpeed = 120.0f;
        float defaultSlewSpeed = 30.0f;
        float acceleration = 50.0f;
        float joystickDeadZone = 0.05f;
    };

    struct BallisticsConfig {
        float maxZeroingOffset = 10.0f;
        float zeroingStepSize = 0.1f;
        float maxWindSpeed = 50.0f;
        float windStepSize = 1.0f;
        float defaultBulletSpeed = 850.0f;
    };

    struct UiConfig {
        int osdRefreshRate = 30;
        QString defaultReticle = "BoxCrosshair";
        int fontSize = 14;
        bool enableStatusOverlay = true;
        bool showDebugInfo = false;
    };

    struct SafetyConfig {
        bool enableNoFireZones = true;
        bool enableNoTraverseZones = true;
        bool requireArmedState = true;
        bool requireStationEnabled = true;
        float motorMaxTemp = 80.0f;
        float motorWarningTemp = 70.0f;
        float driverMaxTemp = 85.0f;
        float driverWarningTemp = 75.0f;
    };

    struct PerformanceConfig {
        int gimbalMotionBufferSize = 60000;
        int imuDataBufferSize = 120000;
        int trackingDataBufferSize = 36000;
        int videoFrameBufferSize = 10;
    };

    // Load configuration from file (tries external first, then embedded resource)
    static bool load(const QString& externalPath = "./config/devices.json");

    // Getters - Hardware
    static const VideoConfig& video() {
        return m_video;
    }
    static const ImuConfig& imu() {
        return m_imu;
    }
    static const LrfConfig& lrf() {
        return m_lrf;
    }
    static const PlcConfig& plc21() {
        return m_plc21;
    }
    static const PlcConfig& plc42() {
        return m_plc42;
    }
    static const ServoConfig& servoAz() {
        return m_servoAz;
    }
    static const ServoConfig& servoEl() {
        return m_servoEl;
    }
    static const ActuatorConfig& actuator() {
        return m_actuator;
    }

    // Getters - System
    static const SystemConfig& system() {
        return m_system;
    }
    static const GimbalConfig& gimbal() {
        return m_gimbal;
    }
    static const BallisticsConfig& ballistics() {
        return m_ballistics;
    }
    static const UiConfig& ui() {
        return m_ui;
    }
    static const SafetyConfig& safety() {
        return m_safety;
    }
    static const PerformanceConfig& performance() {
        return m_performance;
    }

private:
    static bool loadFromFile(const QString& filePath);
    static QSerialPort::Parity parseParity(const QString& parityStr);

    static VideoConfig m_video;
    static ImuConfig m_imu;
    static LrfConfig m_lrf;
    static PlcConfig m_plc21;
    static PlcConfig m_plc42;
    static ServoConfig m_servoAz;
    static ServoConfig m_servoEl;
    static ActuatorConfig m_actuator;
    static SystemConfig m_system;
    static GimbalConfig m_gimbal;
    static BallisticsConfig m_ballistics;
    static UiConfig m_ui;
    static SafetyConfig m_safety;
    static PerformanceConfig m_performance;
};

#endif  // DEVICECONFIGURATION_H

#ifndef HARDWAREMANAGER_H
#define HARDWAREMANAGER_H

#include <QObject>
#include <QThread>

// Forward declarations - Transport & Parsers
class Transport;
class ModbusTransport;
class SerialPortTransport;
class Imu3DMGX3ProtocolParser;
class DayCameraProtocolParser;
class NightCameraProtocolParser;
class JoystickProtocolParser;
class Plc21ProtocolParser;
class Plc42ProtocolParser;
class ServoDriverProtocolParser;
class ServoActuatorProtocolParser;
class LrfProtocolParser;
class RadarProtocolParser;

// Forward declarations - Hardware Devices
class DayCameraControlDevice;
class CameraVideoStreamDevice;
class ImuDevice;
class JoystickDevice;
class LensDevice;
class LRFDevice;
class NightCameraControlDevice;
class Plc21Device;
class Plc42Device;
class RadarDevice;
class ServoActuatorDevice;
class ServoDriverDevice;

// Forward declarations - Data Models
class DayCameraDataModel;
class GyroDataModel;
class JoystickDataModel;
class LensDataModel;
class LrfDataModel;
class NightCameraDataModel;
class Plc21DataModel;
class Plc42DataModel;
class RadarDataModel;
class ServoActuatorDataModel;
class ServoDriverDataModel;
class SystemStateModel;

/**
 * @class HardwareManager
 * @brief Manages all hardware devices, transports, parsers, and data models.
 *
 * This class encapsulates the MIL-STD three-layer architecture:
 * - Transport Layer (I/O operations)
 * - Protocol Layer (Message parsing)
 * - Device Layer (Business logic)
 *
 * It also manages data models that bridge devices to the SystemStateModel.
 */
class HardwareManager : public QObject {
    Q_OBJECT

public:
    explicit HardwareManager(SystemStateModel* systemStateModel, QObject* parent = nullptr);
    ~HardwareManager();

    // ============================================================================
    // INITIALIZATION PHASES
    // ============================================================================

    /**
     * @brief Phase 1: Create all transport, parser, and device objects
     * @return true if successful
     */
    bool createHardware();

    /**
     * @brief Phase 2: Connect device signals to data models
     * @return true if successful
     */
    bool connectDevicesToModels();

    /**
     * @brief Phase 3: Connect data models to SystemStateModel
     * @return true if successful
     */
    bool connectModelsToSystemState();

    /**
     * @brief Phase 4: Open transport connections and initialize devices
     * @return true if successful
     */
    bool startHardware();

    // ============================================================================
    // DEVICE ACCESSORS (for controllers to access hardware)
    // ============================================================================

    // Camera devices
    DayCameraControlDevice* dayCameraControl() const {
        return m_dayCamControl;
    }
    CameraVideoStreamDevice* dayVideoProcessor() const {
        return m_dayVideoProcessor;
    }
    NightCameraControlDevice* nightCameraControl() const {
        return m_nightCamControl;
    }
    CameraVideoStreamDevice* nightVideoProcessor() const {
        return m_nightVideoProcessor;
    }

    // Sensor devices
    ImuDevice* imuDevice() const {
        return m_gyroDevice;
    }
    JoystickDevice* joystickDevice() const {
        return m_joystickDevice;
    }
    LRFDevice* lrfDevice() const {
        return m_lrfDevice;
    }
    RadarDevice* radarDevice() const {
        return m_radarDevice;
    }

    // PLC devices
    Plc21Device* plc21Device() const {
        return m_plc21Device;
    }
    Plc42Device* plc42Device() const {
        return m_plc42Device;
    }

    // Servo devices
    ServoDriverDevice* servoAzDevice() const {
        return m_servoAzDevice;
    }
    ServoDriverDevice* servoElDevice() const {
        return m_servoElDevice;
    }
    ServoActuatorDevice* servoActuatorDevice() const {
        return m_servoActuatorDevice;
    }

    // Data Models
    SystemStateModel* systemStateModel() const {
        return m_systemStateModel;
    }
    JoystickDataModel* joystickDataModel() const {
        return m_joystickModel;
    }

signals:
    void hardwareInitialized();
    void hardwareStarted();
    void hardwareError(const QString& errorMessage);

private:
    // Helper methods
    void createTransportLayer();
    void createProtocolParsers();
    void createDevices();
    void createDataModels();
    void openTransports();
    void initializeDevices();
    void configureCameraDefaults();

    // ============================================================================
    // TRANSPORT LAYER
    // ============================================================================
    SerialPortTransport* m_imuTransport = nullptr;  // 3DM-GX3-25 uses serial binary
    SerialPortTransport* m_dayCameraTransport = nullptr;
    SerialPortTransport* m_nightCameraTransport = nullptr;
    SerialPortTransport* m_lrfTransport = nullptr;
    SerialPortTransport* m_radarTransport = nullptr;
    ModbusTransport* m_plc21Transport = nullptr;
    ModbusTransport* m_plc42Transport = nullptr;
    ModbusTransport* m_servoAzTransport = nullptr;
    ModbusTransport* m_servoElTransport = nullptr;
    SerialPortTransport* m_servoActuatorTransport = nullptr;

    // ============================================================================
    // PROTOCOL PARSERS
    // ============================================================================
    Imu3DMGX3ProtocolParser* m_imuParser = nullptr;
    DayCameraProtocolParser* m_dayCameraParser = nullptr;
    NightCameraProtocolParser* m_nightCameraParser = nullptr;
    JoystickProtocolParser* m_joystickParser = nullptr;
    LrfProtocolParser* m_lrfParser = nullptr;
    RadarProtocolParser* m_radarParser = nullptr;
    Plc21ProtocolParser* m_plc21Parser = nullptr;
    Plc42ProtocolParser* m_plc42Parser = nullptr;
    ServoDriverProtocolParser* m_servoAzParser = nullptr;
    ServoDriverProtocolParser* m_servoElParser = nullptr;
    ServoActuatorProtocolParser* m_servoActuatorParser = nullptr;

    // ============================================================================
    // HARDWARE DEVICES
    // ============================================================================
    DayCameraControlDevice* m_dayCamControl = nullptr;
    CameraVideoStreamDevice* m_dayVideoProcessor = nullptr;
    ImuDevice* m_gyroDevice = nullptr;
    JoystickDevice* m_joystickDevice = nullptr;
    LensDevice* m_lensDevice = nullptr;
    LRFDevice* m_lrfDevice = nullptr;
    NightCameraControlDevice* m_nightCamControl = nullptr;
    CameraVideoStreamDevice* m_nightVideoProcessor = nullptr;
    Plc21Device* m_plc21Device = nullptr;
    Plc42Device* m_plc42Device = nullptr;
    RadarDevice* m_radarDevice = nullptr;
    ServoActuatorDevice* m_servoActuatorDevice = nullptr;
    ServoDriverDevice* m_servoAzDevice = nullptr;
    ServoDriverDevice* m_servoElDevice = nullptr;

    // ============================================================================
    // DEVICE THREADS
    // ============================================================================
    QThread* m_servoAzThread = nullptr;
    QThread* m_servoElThread = nullptr;

    // ============================================================================
    // DATA MODELS
    // ============================================================================
    DayCameraDataModel* m_dayCamControlModel = nullptr;
    GyroDataModel* m_gyroModel = nullptr;
    JoystickDataModel* m_joystickModel = nullptr;
    LensDataModel* m_lensModel = nullptr;
    LrfDataModel* m_lrfModel = nullptr;
    NightCameraDataModel* m_nightCamControlModel = nullptr;
    Plc21DataModel* m_plc21Model = nullptr;
    Plc42DataModel* m_plc42Model = nullptr;
    RadarDataModel* m_radarModel = nullptr;
    ServoActuatorDataModel* m_servoActuatorModel = nullptr;
    ServoDriverDataModel* m_servoAzModel = nullptr;
    ServoDriverDataModel* m_servoElModel = nullptr;

    // Reference to SystemStateModel (not owned)
    SystemStateModel* m_systemStateModel = nullptr;
};

#endif  // HARDWAREMANAGER_H

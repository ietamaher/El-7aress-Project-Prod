#include "HardwareManager.h"

// Hardware Devices
#include "hardware/devices/daycameracontroldevice.h"
#include "hardware/devices/cameravideostreamdevice.h"
#include "hardware/devices/imudevice.h"
#include "hardware/devices/joystickdevice.h"
#include "hardware/devices/lrfdevice.h"
#include "hardware/devices/nightcameracontroldevice.h"
#include "hardware/devices/plc21device.h"
#include "hardware/devices/plc42device.h"
#include "hardware/devices/radardevice.h"
#include "hardware/devices/servoactuatordevice.h"
#include "hardware/devices/servodriverdevice.h"

// Transport & Protocol Parsers
#include "hardware/communication/modbustransport.h"
#include "hardware/communication/serialporttransport.h"
#include "hardware/protocols/Imu3DMGX3ProtocolParser.h"
#include "hardware/protocols/DayCameraProtocolParser.h"
#include "hardware/protocols/NightCameraProtocolParser.h"
#include "hardware/protocols/JoystickProtocolParser.h"
#include "hardware/protocols/LrfProtocolParser.h"
#include "hardware/protocols/RadarProtocolParser.h"
#include "hardware/protocols/Plc21ProtocolParser.h"
#include "hardware/protocols/Plc42ProtocolParser.h"
#include "hardware/protocols/ServoDriverProtocolParser.h"
#include "hardware/protocols/ServoActuatorProtocolParser.h"

// Data Models
#include "models/domain/daycameradatamodel.h"
#include "models/domain/nightcameradatamodel.h"
#include "models/domain/gyrodatamodel.h"
#include "models/domain/joystickdatamodel.h"
#include "models/domain/lrfdatamodel.h"
#include "models/domain/radardatamodel.h"
#include "models/domain/plc21datamodel.h"
#include "models/domain/plc42datamodel.h"
#include "models/domain/servoactuatordatamodel.h"
#include "models/domain/servodriverdatamodel.h"
#include "models/domain/systemstatemodel.h"

// Configuration
#include "controllers/deviceconfiguration.h"

#include <QDebug>
#include <QJsonObject>
#include <QSerialPort>

HardwareManager::HardwareManager(SystemStateModel* systemStateModel, QObject* parent)
    : QObject(parent),
      m_systemStateModel(systemStateModel)
{
    if (!m_systemStateModel) {
        qCritical() << "HardwareManager: SystemStateModel is null!";
    }
}

HardwareManager::~HardwareManager()
{
    qInfo() << "HardwareManager: Shutting down...";

    // CRITICAL FIX: Handle thread cleanup with proper timeout recovery
    // Military systems must shutdown gracefully without resource leaks

    // Stop video processors
    if (m_dayVideoProcessor && m_dayVideoProcessor->isRunning()) {
        m_dayVideoProcessor->stop();
        if (!m_dayVideoProcessor->wait(2000)) {
            qWarning() << "Day video processor did not stop gracefully - forcing termination";
            m_dayVideoProcessor->terminate();
            if (!m_dayVideoProcessor->wait(1000)) {
                qCritical() << "Failed to terminate day video processor - RESOURCE LEAK!";
            }
        } else {
            qInfo() << "  ✓ Day video processor stopped gracefully";
        }
    }

    if (m_nightVideoProcessor && m_nightVideoProcessor->isRunning()) {
        m_nightVideoProcessor->stop();
        if (!m_nightVideoProcessor->wait(2000)) {
            qWarning() << "Night video processor did not stop gracefully - forcing termination";
            m_nightVideoProcessor->terminate();
            if (!m_nightVideoProcessor->wait(1000)) {
                qCritical() << "Failed to terminate night video processor - RESOURCE LEAK!";
            }
        } else {
            qInfo() << "  ✓ Night video processor stopped gracefully";
        }
    }

    // Stop servo threads
    if (m_servoAzThread && m_servoAzThread->isRunning()) {
        m_servoAzThread->quit();
        if (!m_servoAzThread->wait(1000)) {
            qWarning() << "Servo azimuth thread did not quit gracefully - forcing termination";
            m_servoAzThread->terminate();
            if (!m_servoAzThread->wait(1000)) {
                qCritical() << "Failed to terminate servo azimuth thread - RESOURCE LEAK!";
            }
        } else {
            qInfo() << "  ✓ Servo azimuth thread stopped gracefully";
        }
    }

    if (m_servoElThread && m_servoElThread->isRunning()) {
        m_servoElThread->quit();
        if (!m_servoElThread->wait(1000)) {
            qWarning() << "Servo elevation thread did not quit gracefully - forcing termination";
            m_servoElThread->terminate();
            if (!m_servoElThread->wait(1000)) {
                qCritical() << "Failed to terminate servo elevation thread - RESOURCE LEAK!";
            }
        } else {
            qInfo() << "  ✓ Servo elevation thread stopped gracefully";
        }
    }

    qInfo() << "HardwareManager: Shutdown complete.";
}

// ============================================================================
// PUBLIC INITIALIZATION METHODS
// ============================================================================

bool HardwareManager::createHardware()
{
    qInfo() << "=== HardwareManager: Creating Hardware ===";

    try {
        createTransportLayer();
        createProtocolParsers();
        createDevices();
        createDataModels();

        qInfo() << "  ✓ Hardware creation complete";
        emit hardwareInitialized();
        return true;

    } catch (const std::exception& e) {
        QString errorMsg = QString("Hardware creation failed: %1").arg(e.what());
        qCritical() << errorMsg;
        emit hardwareError(errorMsg);
        return false;
    }
}

bool HardwareManager::connectDevicesToModels()
{
    qInfo() << "=== HardwareManager: Connecting Devices to Models ===";

    connect(m_dayCamControl, &DayCameraControlDevice::dayCameraDataChanged,
            m_dayCamControlModel, &DayCameraDataModel::updateData);

    connect(m_gyroDevice, &ImuDevice::imuDataChanged,
            m_gyroModel, &GyroDataModel::updateData);

    connect(m_joystickDevice, &JoystickDevice::axisMoved,
            m_joystickModel, &JoystickDataModel::onRawAxisMoved);
    connect(m_joystickDevice, &JoystickDevice::buttonPressed,
            m_joystickModel, &JoystickDataModel::onRawButtonChanged);
    connect(m_joystickDevice, &JoystickDevice::hatMoved,
            m_joystickModel, &JoystickDataModel::onRawHatMoved);

    // LRFDevice uses shared_ptr
    connect(m_lrfDevice, &LRFDevice::lrfDataChanged,
            m_lrfModel, [this](std::shared_ptr<const LrfData> data) {
                if (data) {
                    m_lrfModel->updateData(*data);
                }
            });

    connect(m_nightCamControl, &NightCameraControlDevice::nightCameraDataChanged,
            m_nightCamControlModel, &NightCameraDataModel::updateData);

    connect(m_plc21Device, &Plc21Device::panelDataChanged,
            m_plc21Model, &Plc21DataModel::updateData);

    connect(m_plc42Device, &Plc42Device::plc42DataChanged,
            m_plc42Model, &Plc42DataModel::updateData);

    connect(m_servoActuatorDevice, &ServoActuatorDevice::actuatorDataChanged,
            m_servoActuatorModel, &ServoActuatorDataModel::updateData);

    connect(m_servoAzDevice, &ServoDriverDevice::servoDataChanged,
            m_servoAzModel, &ServoDriverDataModel::updateData);

    connect(m_servoElDevice, &ServoDriverDevice::servoDataChanged,
            m_servoElModel, &ServoDriverDataModel::updateData);

    qInfo() << "  ✓ Devices connected to models";
    return true;
}

bool HardwareManager::connectModelsToSystemState()
{
    qInfo() << "=== HardwareManager: Connecting Models to SystemState ===";

    if (!m_systemStateModel) {
        qCritical() << "Cannot connect models: SystemStateModel is null!";
        return false;
    }

    connect(m_dayCamControlModel, &DayCameraDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onDayCameraDataChanged);

    connect(m_gyroModel, &GyroDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onGyroDataChanged);

    connect(m_joystickModel, &JoystickDataModel::axisMoved,
            m_systemStateModel, &SystemStateModel::onJoystickAxisChanged);
    connect(m_joystickModel, &JoystickDataModel::buttonPressed,
            m_systemStateModel, &SystemStateModel::onJoystickButtonChanged);
    connect(m_joystickModel, &JoystickDataModel::hatMoved,
            m_systemStateModel, &SystemStateModel::onJoystickHatChanged);

    // Connect joystick device data directly for connection status
    connect(m_joystickDevice, &JoystickDevice::dataChanged,
            m_systemStateModel, &SystemStateModel::onJoystickDataChanged);

    connect(m_lrfModel, &LrfDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onLrfDataChanged);

    connect(m_nightCamControlModel, &NightCameraDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onNightCameraDataChanged);

    connect(m_plc21Model, &Plc21DataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onPlc21DataChanged);

    connect(m_plc42Model, &Plc42DataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onPlc42DataChanged);

    connect(m_servoActuatorModel, &ServoActuatorDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onServoActuatorDataChanged);

    connect(m_servoAzModel, &ServoDriverDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onServoAzDataChanged);

    connect(m_servoElModel, &ServoDriverDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onServoElDataChanged);

    // Connect SystemStateModel back to cameras
    if (m_dayVideoProcessor) {
        connect(m_systemStateModel, &SystemStateModel::dataChanged,
                m_dayVideoProcessor, &CameraVideoStreamDevice::onSystemStateChanged,
                Qt::QueuedConnection);
    }

    if (m_nightVideoProcessor) {
        connect(m_systemStateModel, &SystemStateModel::dataChanged,
                m_nightVideoProcessor, &CameraVideoStreamDevice::onSystemStateChanged,
                Qt::QueuedConnection);
    }

    qInfo() << "  ✓ Models connected to SystemStateModel";
    return true;
}

bool HardwareManager::startHardware()
{
    qInfo() << "=== HardwareManager: Starting Hardware ===";

    try {
        openTransports();
        initializeDevices();
        configureCameraDefaults();

        // Start video processing threads
        if (m_dayVideoProcessor) {
            m_dayVideoProcessor->start();
            qInfo() << "  ✓ Day camera thread started";
        }

        if (m_nightVideoProcessor) {
            m_nightVideoProcessor->start();
            qInfo() << "  ✓ Night camera thread started";
        }

        qInfo() << "  ✓ Hardware started successfully";
        emit hardwareStarted();
        return true;

    } catch (const std::exception& e) {
        QString errorMsg = QString("Hardware startup failed: %1").arg(e.what());
        qCritical() << errorMsg;
        emit hardwareError(errorMsg);
        return false;
    }
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

void HardwareManager::createTransportLayer()
{
    qInfo() << "  Creating transport layer...";

    m_imuTransport = new SerialPortTransport(this);  // 3DM-GX3-25 uses serial binary, not Modbus
    m_dayCameraTransport = new SerialPortTransport(this);
    m_nightCameraTransport = new SerialPortTransport(this);
    m_lrfTransport = new SerialPortTransport(this);
    m_radarTransport = new SerialPortTransport(this);
    m_plc21Transport = new ModbusTransport(this);
    m_plc42Transport = new ModbusTransport(this);
    m_servoAzTransport = new ModbusTransport(this);
    m_servoElTransport = new ModbusTransport(this);
    m_servoActuatorTransport = new SerialPortTransport(this);

    qInfo() << "    ✓ Transport layer created";
}

void HardwareManager::createProtocolParsers()
{
    qInfo() << "  Creating protocol parsers...";

    m_imuParser = new Imu3DMGX3ProtocolParser(this);
    m_dayCameraParser = new DayCameraProtocolParser(this);
    m_nightCameraParser = new NightCameraProtocolParser(this);
    m_joystickParser = new JoystickProtocolParser(this);
    m_lrfParser = new LrfProtocolParser(this);
    m_radarParser = new RadarProtocolParser(this);
    m_plc21Parser = new Plc21ProtocolParser(this);
    m_plc42Parser = new Plc42ProtocolParser(this);
    m_servoAzParser = new ServoDriverProtocolParser(this);
    m_servoElParser = new ServoDriverProtocolParser(this);
    m_servoActuatorParser = new ServoActuatorProtocolParser(this);

    qInfo() << "    ✓ Protocol parsers created";
}

void HardwareManager::createDevices()
{
    qInfo() << "  Creating devices...";

    const auto& videoConf = DeviceConfiguration::video();
    const auto& imuConf = DeviceConfiguration::imu();
    const auto& servoAzConf = DeviceConfiguration::servoAz();
    const auto& servoElConf = DeviceConfiguration::servoEl();

    // Day Camera (Pelco-D via Serial)
    m_dayCamControl = new DayCameraControlDevice("dayCamera", this);
    m_dayCamControl->setDependencies(m_dayCameraTransport, m_dayCameraParser);

    // IMU (3DM-GX3-25 - Serial Binary Protocol)
    m_gyroDevice = new ImuDevice("imu", this);
    m_gyroDevice->setDependencies(m_imuTransport, m_imuParser);

    // Pass IMU configuration (sampling rate, tilt threshold)
    QJsonObject imuConfig;
    imuConfig["samplingRateHz"] = imuConf.samplingRateHz;
    imuConfig["tiltWarningThreshold"] = imuConf.tiltWarningThreshold;
    qInfo() << "  [IMU Config] samplingRateHz =" << imuConf.samplingRateHz << "Hz, tiltWarningThreshold =" << imuConf.tiltWarningThreshold << "deg";
    m_gyroDevice->setProperty("config", imuConfig);

    // Joystick (SDL2 - no transport needed)
    m_joystickDevice = new JoystickDevice(this);
    m_joystickDevice->setParser(m_joystickParser);

    // LRF (Serial binary protocol)
    m_lrfDevice = new LRFDevice(this);
    m_lrfDevice->setDependencies(m_lrfTransport, m_lrfParser);

    // Night Camera (TAU2 via Serial)
    m_nightCamControl = new NightCameraControlDevice("nightCamera", this);
    m_nightCamControl->setDependencies(m_nightCameraTransport, m_nightCameraParser);

    // Radar (NMEA 0183 via Serial)
    //m_radarDevice = new RadarDevice("radar", this);
    //m_radarDevice->setDependencies(m_radarTransport, m_radarParser);

    // PLC21 (Modbus RTU)
    m_plc21Device = new Plc21Device("plc21", this);
    m_plc21Device->setDependencies(m_plc21Transport, m_plc21Parser);

    // PLC42 (Modbus RTU)
    m_plc42Device = new Plc42Device("plc42", this);
    m_plc42Device->setDependencies(m_plc42Transport, m_plc42Parser);

    // Servo Actuator (Serial ASCII protocol)
    m_servoActuatorDevice = new ServoActuatorDevice("servoActuator", this);
    m_servoActuatorDevice->setDependencies(m_servoActuatorTransport, m_servoActuatorParser);

    // Servo Driver devices (Modbus RTU) with MIL-STD architecture
    m_servoAzThread = new QThread(this);
    m_servoAzDevice = new ServoDriverDevice(servoAzConf.name, nullptr);
    m_servoAzDevice->setDependencies(m_servoAzTransport, m_servoAzParser);

    m_servoElThread = new QThread(this);
    m_servoElDevice = new ServoDriverDevice(servoElConf.name, nullptr);
    m_servoElDevice->setDependencies(m_servoElTransport, m_servoElParser);

    // Video processors with configuration
    m_dayVideoProcessor = new CameraVideoStreamDevice(
        0, videoConf.dayDevicePath, videoConf.sourceWidth,
        videoConf.sourceHeight, m_systemStateModel, nullptr);

    m_nightVideoProcessor = new CameraVideoStreamDevice(
        1, videoConf.nightDevicePath, videoConf.sourceWidth,
        videoConf.sourceHeight, m_systemStateModel, nullptr);

    qInfo() << "    ✓ Devices created with dependency injection";
}

void HardwareManager::createDataModels()
{
    qInfo() << "  Creating data models...";

    m_dayCamControlModel = new DayCameraDataModel(this);
    m_gyroModel = new GyroDataModel(this);
    m_joystickModel = new JoystickDataModel(this);
    m_lrfModel = new LrfDataModel(this);
    m_nightCamControlModel = new NightCameraDataModel(this);
    m_plc21Model = new Plc21DataModel(this);
    m_plc42Model = new Plc42DataModel(this);
    m_radarModel = new RadarDataModel(this);
    m_servoActuatorModel = new ServoActuatorDataModel(this);
    m_servoAzModel = new ServoDriverDataModel(this);
    m_servoElModel = new ServoDriverDataModel(this);

    qInfo() << "    ✓ Data models created";
}

void HardwareManager::openTransports()
{
    qInfo() << "  Opening transport connections...";

    const auto& videoConf = DeviceConfiguration::video();
    const auto& imuConf = DeviceConfiguration::imu();
    const auto& lrfConf = DeviceConfiguration::lrf();
    const auto& plc21Conf = DeviceConfiguration::plc21();
    const auto& plc42Conf = DeviceConfiguration::plc42();
    const auto& actuatorConf = DeviceConfiguration::actuator();
    const auto& servoAzConf = DeviceConfiguration::servoAz();
    const auto& servoElConf = DeviceConfiguration::servoEl();

    // IMU Transport (Serial Binary - 3DM-GX3-25)
    QJsonObject imuTransportConfig;
    imuTransportConfig["port"] = imuConf.port;
    imuTransportConfig["baudRate"] = imuConf.baudRate;
    imuTransportConfig["parity"] = static_cast<int>(QSerialPort::NoParity);
    // Note: No slaveId for serial binary protocol (not Modbus)
    m_imuTransport->open(imuTransportConfig);

    // Day Camera Transport (Serial)
    QJsonObject dayCameraTransportConfig;
    dayCameraTransportConfig["port"] = videoConf.dayControlPort;
    dayCameraTransportConfig["baudRate"] = 9600;  // Pelco-D standard
    dayCameraTransportConfig["parity"] = static_cast<int>(QSerialPort::NoParity);
    m_dayCameraTransport->open(dayCameraTransportConfig);

    // Night Camera Transport (Serial)
    QJsonObject nightCameraTransportConfig;
    nightCameraTransportConfig["port"] = videoConf.nightControlPort;
    nightCameraTransportConfig["baudRate"] = 921600;  // FLIR Boson 640 standard
    nightCameraTransportConfig["parity"] = static_cast<int>(QSerialPort::NoParity);
    m_nightCameraTransport->open(nightCameraTransportConfig);

    // PLC21 Transport (Modbus RTU)
    QJsonObject plc21TransportConfig;
    plc21TransportConfig["port"] = plc21Conf.port;
    plc21TransportConfig["baudRate"] = plc21Conf.baudRate;
    plc21TransportConfig["parity"] = static_cast<int>(plc21Conf.parity);
    plc21TransportConfig["slaveId"] = plc21Conf.slaveId;
    m_plc21Transport->open(plc21TransportConfig);

    // PLC42 Transport (Modbus RTU)
    QJsonObject plc42TransportConfig;
    plc42TransportConfig["port"] = plc42Conf.port;
    plc42TransportConfig["baudRate"] = plc42Conf.baudRate;
    plc42TransportConfig["parity"] = static_cast<int>(plc42Conf.parity);
    plc42TransportConfig["slaveId"] = plc42Conf.slaveId;
    m_plc42Transport->open(plc42TransportConfig);

    // Servo Azimuth Transport (Modbus RTU)
    QJsonObject servoAzTransportConfig;
    servoAzTransportConfig["port"] = servoAzConf.port;
    servoAzTransportConfig["baudRate"] = servoAzConf.baudRate;
    servoAzTransportConfig["parity"] = static_cast<int>(servoAzConf.parity);
    servoAzTransportConfig["slaveId"] = servoAzConf.slaveId;
    m_servoAzTransport->open(servoAzTransportConfig);

    // Servo Elevation Transport (Modbus RTU)
    QJsonObject servoElTransportConfig;
    servoElTransportConfig["port"] = servoElConf.port;
    servoElTransportConfig["baudRate"] = servoElConf.baudRate;
    servoElTransportConfig["parity"] = static_cast<int>(servoElConf.parity);
    servoElTransportConfig["slaveId"] = servoElConf.slaveId;
    m_servoElTransport->open(servoElTransportConfig);

    // Servo Actuator Transport (Serial)
    QJsonObject servoActuatorTransportConfig;
    servoActuatorTransportConfig["port"] = actuatorConf.port;
    servoActuatorTransportConfig["baudRate"] = actuatorConf.baudRate;
    servoActuatorTransportConfig["parity"] = static_cast<int>(QSerialPort::NoParity);
    m_servoActuatorTransport->open(servoActuatorTransportConfig);

    // LRF Transport (Serial binary protocol)
    QJsonObject lrfTransportConfig;
    lrfTransportConfig["port"] = lrfConf.port;
    lrfTransportConfig["baudRate"] = lrfConf.baudRate;
    lrfTransportConfig["parity"] = static_cast<int>(QSerialPort::NoParity);
    m_lrfTransport->open(lrfTransportConfig);

    qInfo() << "    ✓ Transport connections opened";
}

void HardwareManager::initializeDevices()
{
    qInfo() << "  Initializing devices...";

    m_dayCamControl->initialize();
    m_gyroDevice->initialize();
    m_joystickDevice->initialize();
    m_nightCamControl->initialize();
    m_plc21Device->initialize();
    m_plc42Device->initialize();
    m_lrfDevice->initialize();
   // m_radarDevice->initialize();
    m_servoActuatorDevice->initialize();

    if (m_servoAzDevice) m_servoAzDevice->initialize();
    if (m_servoElDevice) m_servoElDevice->initialize();

    qInfo() << "    ✓ All devices initialized";
}

void HardwareManager::configureCameraDefaults()
{
    qInfo() << "  Configuring camera defaults...";

    m_dayCamControl->zoomOut();
    m_dayCamControl->zoomStop();
    m_nightCamControl->setDigitalZoom(0);

    qInfo() << "    ✓ Camera defaults configured";
}

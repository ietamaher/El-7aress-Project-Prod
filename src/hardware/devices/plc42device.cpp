#include "plc42device.h"
#include "../interfaces/Transport.h"
#include "../protocols/Plc42ProtocolParser.h"
#include "../messages/Plc42Message.h"
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>

Plc42Device::Plc42Device(const QString& identifier, QObject* parent)
    : TemplatedDevice<Plc42Data>(parent),
      m_identifier(identifier),
      m_pollTimer(new QTimer(this)),
      m_communicationWatchdog(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &Plc42Device::pollTimerTimeout);

    m_communicationWatchdog->setSingleShot(false);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout,
            this, &Plc42Device::onCommunicationWatchdogTimeout);
}

Plc42Device::~Plc42Device() {
    m_pollTimer->stop();
    m_communicationWatchdog->stop();
}

void Plc42Device::setDependencies(Transport* transport,
                                   Plc42ProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    // Parent them to this device for lifetime management
    m_transport->setParent(this);
    m_parser->setParent(this);

    // Don't listen to transport connectionStateChanged - we manage connection via watchdog
}

bool Plc42Device::initialize() {
    setState(DeviceState::Initializing);

    if (!m_transport || !m_parser) {
        qCritical() << m_identifier << "missing dependencies!";
        setState(DeviceState::Error);
        return false;
    }

    // Transport should already be opened by SystemController
    qDebug() << m_identifier << "initializing...";

    // Get poll interval from config (default 50ms)
    QJsonObject config = property("config").toJsonObject();
    int pollInterval = config["pollIntervalMs"].toInt(50);
    m_pollTimer->setInterval(pollInterval);

    setState(DeviceState::Online);

    // Start watchdog
    m_communicationWatchdog->start();

    // Start first poll cycle immediately
    startPollCycle();

    qDebug() << m_identifier << "initialized successfully with poll interval:" << pollInterval << "ms";
    return true;
}

void Plc42Device::shutdown() {
    m_pollTimer->stop();
    m_communicationWatchdog->stop();

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    setState(DeviceState::Offline);
}

void Plc42Device::pollTimerTimeout() {
    // Timer fired - start a new poll cycle
    startPollCycle();
}

void Plc42Device::startPollCycle() {
    // Don't start a new cycle if one is already in progress
    // This implements adaptive polling - we wait for the previous cycle to complete
    if (m_pollCycleActive) {
        return;
    }

    m_pollCycleActive = true;
    m_needsHoldingRegistersRead = true;
    m_waitingForResponse = true;

    // Start the request sequence: first read digital inputs
    sendReadRequest(Plc42Registers::DIGITAL_INPUTS_START_ADDR,
                    8,  // Read 8 discrete inputs
                    true);
}

void Plc42Device::sendReadRequest(int startAddress, int count, bool isDiscreteInputs) {
    if (state() != DeviceState::Online || !m_transport) return;

    // Cast to ModbusTransport to access Modbus-specific methods
    auto modbusTransport = qobject_cast<QModbusRtuSerialClient*>(
        m_transport->property("client").value<QObject*>());

    if (!modbusTransport) return;

    QModbusDataUnit::RegisterType regType = isDiscreteInputs ?
        QModbusDataUnit::DiscreteInputs : QModbusDataUnit::HoldingRegisters;

    QModbusDataUnit readUnit(regType, startAddress, count);

    QModbusReply* reply = nullptr;
    QMetaObject::invokeMethod(m_transport, "sendReadRequest",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(QModbusReply*, reply),
                              Q_ARG(QModbusDataUnit, readUnit));

    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            onModbusReplyReady(reply);
        });
    }
}

void Plc42Device::onModbusReplyReady(QModbusReply* reply) {
    if (!reply || !m_parser) {
        if (reply) reply->deleteLater();
        m_waitingForResponse = false;
        m_pollCycleActive = false;  // Abort cycle on error
        m_needsHoldingRegistersRead = false;
        m_pollTimer->start();  // Schedule retry
        return;
    }

    if (reply->error() != QModbusDevice::NoError) {
        //qWarning() << m_identifier << "Modbus error:" << reply->errorString();
        setConnectionState(false);
        reply->deleteLater();
        m_waitingForResponse = false;
        m_pollCycleActive = false;  // Abort cycle on error
        m_needsHoldingRegistersRead = false;
        m_pollTimer->start();  // Schedule retry
        return;
    }

    // Parse the reply into messages
    auto messages = m_parser->parse(reply);
    reply->deleteLater();

    // Process each message
    for (const auto& msg : messages) {
        if (msg) {
            processMessage(*msg);
        }
    }

    // Response received successfully - send next pending request if any
    m_waitingForResponse = false;
    sendNextPendingRequest();
}

void Plc42Device::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::Plc42DataType) {
        auto const* dataMsg = static_cast<const Plc42DataMessage*>(&message);
        mergePartialData(dataMsg->data());
    }
}

void Plc42Device::mergePartialData(const Plc42Data& partialData) {
    // We received valid data - device is connected and communicating
    setConnectionState(true);
    resetCommunicationWatchdog();

    auto currentData = data();
    auto newData = std::make_shared<Plc42Data>(*currentData);

    bool dataChanged = false;

    // ========================================================================
    // Merge discrete inputs (8 total - includes dual HOME-END)
    // ========================================================================
    if (partialData.stationUpperSensor != currentData->stationUpperSensor ||
        partialData.stationLowerSensor != currentData->stationLowerSensor ||
        partialData.emergencyStopActive != currentData->emergencyStopActive ||
        partialData.ammunitionLevel != currentData->ammunitionLevel ||
        partialData.hatchState != currentData->hatchState ||
        partialData.freeGimbalState != currentData->freeGimbalState ||
        partialData.azimuthHomeComplete != currentData->azimuthHomeComplete ||    // ⭐ NEW
        partialData.elevationHomeComplete != currentData->elevationHomeComplete || // ⭐ NEW
        partialData.solenoidActive != currentData->solenoidActive) {

        newData->stationUpperSensor = partialData.stationUpperSensor;
        newData->stationLowerSensor = partialData.stationLowerSensor;
        newData->emergencyStopActive = partialData.emergencyStopActive;
        newData->ammunitionLevel = partialData.ammunitionLevel;
        newData->hatchState = partialData.hatchState;
        newData->freeGimbalState = partialData.freeGimbalState;
        newData->azimuthHomeComplete = partialData.azimuthHomeComplete;      // ⭐ NEW
        newData->elevationHomeComplete = partialData.elevationHomeComplete;  // ⭐ NEW
        newData->solenoidActive = partialData.solenoidActive;
        dataChanged = true;
    }

    // ========================================================================
    // Merge holding registers (unchanged)
    // ========================================================================
    if (partialData.solenoidMode != currentData->solenoidMode ||
        partialData.gimbalOpMode != currentData->gimbalOpMode ||
        partialData.azimuthSpeed != currentData->azimuthSpeed ||
        partialData.elevationSpeed != currentData->elevationSpeed ||
        partialData.azimuthDirection != currentData->azimuthDirection ||
        partialData.elevationDirection != currentData->elevationDirection ||
        partialData.solenoidState != currentData->solenoidState ||
        partialData.resetAlarm != currentData->resetAlarm) {

        newData->solenoidMode = partialData.solenoidMode;
        newData->gimbalOpMode = partialData.gimbalOpMode;
        newData->azimuthSpeed = partialData.azimuthSpeed;
        newData->elevationSpeed = partialData.elevationSpeed;
        newData->azimuthDirection = partialData.azimuthDirection;
        newData->elevationDirection = partialData.elevationDirection;
        newData->solenoidState = partialData.solenoidState;
        newData->resetAlarm = partialData.resetAlarm;
        dataChanged = true;
    }

    if (dataChanged) {
        updateData(newData);
        emit plc42DataChanged(*newData);
    }
}

//================================================================================
// PUBLIC API - CONTROL METHODS
//================================================================================

void Plc42Device::setSolenoidMode(uint16_t mode) {
    auto newData = std::make_shared<Plc42Data>(*data());
    newData->solenoidMode = mode;
    updateData(newData);
    m_hasPendingWrites = true;
    sendWriteHoldingRegisters();
}

void Plc42Device::setSolenoidState(uint16_t state) {
    auto newData = std::make_shared<Plc42Data>(*data());
    newData->solenoidState = state;
    updateData(newData);
    m_hasPendingWrites = true;
    sendWriteHoldingRegisters();
}

void Plc42Device::setResetAlarm(uint16_t alarm) {
    auto newData = std::make_shared<Plc42Data>(*data());
    newData->resetAlarm = alarm;
    updateData(newData);
    m_hasPendingWrites = true;
    sendWriteHoldingRegisters();
}
// setHome position, method
void  Plc42Device::setHomePosition() {
    auto newData = std::make_shared<Plc42Data>(*data());
    newData->gimbalOpMode = 3; // Assuming '3' is the code for 'Home Position' mode
    updateData(newData);
    m_hasPendingWrites = true;
    sendWriteHoldingRegisters();
}

//  setStop gimbal methods
void Plc42Device::setStopGimbal() {
    auto newData = std::make_shared<Plc42Data>(*data());
    newData->gimbalOpMode = 1; // Assuming '1' is the code for 'Stop' mode
    updateData(newData);
    m_hasPendingWrites = true;
    sendWriteHoldingRegisters();
}

void Plc42Device::setManualMode() {
    auto newData = std::make_shared<Plc42Data>(*data());
    newData->gimbalOpMode = 0; // GIMBAL_MANUAL mode
    updateData(newData);
    m_hasPendingWrites = true;
    sendWriteHoldingRegisters();
    qDebug() << m_identifier << "Returning to MANUAL mode (gimbalOpMode = 0)";
}

void Plc42Device::sendWriteHoldingRegisters() {
    if (state() != DeviceState::Online || !m_transport) return;

    auto currentData = data();

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters,
                              Plc42Registers::HOLDING_REGISTERS_START_ADDR,
                              Plc42Registers::HOLDING_REGISTERS_COUNT);

    writeUnit.setValue(0, currentData->solenoidMode);
    writeUnit.setValue(1, currentData->gimbalOpMode);

    // Split 32-bit azimuth speed into two 16-bit registers
    uint16_t azLow  = static_cast<uint16_t>(currentData->azimuthSpeed & 0xFFFF);
    uint16_t azHigh = static_cast<uint16_t>((currentData->azimuthSpeed >> 16) & 0xFFFF);
    writeUnit.setValue(2, azLow);
    writeUnit.setValue(3, azHigh);

    // Split 32-bit elevation speed into two 16-bit registers
    uint16_t elLow  = static_cast<uint16_t>(currentData->elevationSpeed & 0xFFFF);
    uint16_t elHigh = static_cast<uint16_t>((currentData->elevationSpeed >> 16) & 0xFFFF);
    writeUnit.setValue(4, elLow);
    writeUnit.setValue(5, elHigh);

    writeUnit.setValue(6, currentData->azimuthDirection);
    writeUnit.setValue(7, currentData->elevationDirection);
    writeUnit.setValue(8, currentData->solenoidState);
    writeUnit.setValue(9, currentData->resetAlarm);

    QModbusReply* reply = nullptr;
    QMetaObject::invokeMethod(m_transport, "sendWriteRequest",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(QModbusReply*, reply),
                              Q_ARG(QModbusDataUnit, writeUnit));

    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            bool success = (reply->error() == QModbusDevice::NoError);
            if (!success) {
                //qWarning() << m_identifier << "Write error:" << reply->errorString();
            }
            m_hasPendingWrites = false;
            emit registerWritten(success);
            reply->deleteLater();
        });
    }
}

void Plc42Device::setPollInterval(int intervalMs) {
    m_pollTimer->setInterval(intervalMs);
}

void Plc42Device::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void Plc42Device::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<Plc42Data>(*currentData);
        newData->isConnected = connected;
        updateData(newData);
        emit plc42DataChanged(*newData);

        if (connected) {
            qDebug() << m_identifier << "connected";
        } else {
            qWarning() << m_identifier << "disconnected";
        }
    }
}

void Plc42Device::sendNextPendingRequest() {
    // If we need to read holding registers, send that request now
    if (m_needsHoldingRegistersRead) {
        m_needsHoldingRegistersRead = false;
        m_waitingForResponse = true;
        sendReadRequest(Plc42Registers::HOLDING_REGISTERS_START_ADDR,
                        Plc42Registers::HOLDING_REGISTERS_COUNT,
                        false);
    } else {
        // Poll cycle complete - mark as inactive and schedule next cycle
        m_pollCycleActive = false;

        // Start timer for next poll cycle (adaptive polling)
        // Timer will fire after the configured interval
        m_pollTimer->start();
    }
}

void Plc42Device::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
     //          << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

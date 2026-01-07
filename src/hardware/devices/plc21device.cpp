#include "plc21device.h"
#include "../interfaces/Transport.h"
#include "../protocols/Plc21ProtocolParser.h"
#include "../messages/Plc21Message.h"
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>


Plc21Device::Plc21Device(const QString& identifier, QObject* parent)
    : TemplatedDevice<Plc21PanelData>(parent), m_identifier(identifier),
      m_pollTimer(new QTimer(this)), m_communicationWatchdog(new QTimer(this)) {
    connect(m_pollTimer, &QTimer::timeout, this, &Plc21Device::pollTimerTimeout);

    // FIXED: Changed from false to true - watchdog should be single-shot
    // Gets restarted on each successful communication (resetCommunicationWatchdog)
    m_communicationWatchdog->setSingleShot(true);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout, this,
            &Plc21Device::onCommunicationWatchdogTimeout);
}

Plc21Device::~Plc21Device() {
    m_pollTimer->stop();
    m_communicationWatchdog->stop();
}

void Plc21Device::setDependencies(Transport* transport, Plc21ProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    // Parent them to this device for lifetime management
    m_transport->setParent(this);
    m_parser->setParent(this);

    // Don't listen to transport connectionStateChanged - we manage connection via watchdog
}

bool Plc21Device::initialize() {
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

    qDebug() << m_identifier << "initialized successfully with poll interval:" << pollInterval
             << "ms";
    return true;
}

void Plc21Device::shutdown() {
    m_pollTimer->stop();
    m_communicationWatchdog->stop();

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    setState(DeviceState::Offline);
}

void Plc21Device::pollTimerTimeout() {
    // Timer fired - start a new poll cycle
    startPollCycle();
}

void Plc21Device::startPollCycle() {
    // Don't start a new cycle if one is already in progress
    // This implements adaptive polling - we wait for the previous cycle to complete
    if (m_pollCycleActive) {
        return;
    }

    m_pollCycleActive = true;
    m_needsHoldingRegistersRead = true;
    m_waitingForResponse = true;

    // Start the request sequence: first read digital inputs
    sendReadRequest(Plc21Registers::DIGITAL_INPUTS_START_ADDR, Plc21Registers::DIGITAL_INPUTS_COUNT,
                    true);
}

void Plc21Device::sendReadRequest(int startAddress, int count, bool isDiscreteInputs) {
    if (state() != DeviceState::Online || !m_transport)
        return;

    // Cast to ModbusTransport to access Modbus-specific methods
    auto modbusTransport =
        qobject_cast<QModbusRtuSerialClient*>(m_transport->property("client").value<QObject*>());

    if (!modbusTransport)
        return;

    QModbusDataUnit::RegisterType regType =
        isDiscreteInputs ? QModbusDataUnit::DiscreteInputs : QModbusDataUnit::HoldingRegisters;

    QModbusDataUnit readUnit(regType, startAddress, count);

    QModbusReply* reply = nullptr;
    QMetaObject::invokeMethod(m_transport, "sendReadRequest", Qt::DirectConnection,
                              Q_RETURN_ARG(QModbusReply*, reply), Q_ARG(QModbusDataUnit, readUnit));

    if (reply) {
        connect(reply, &QModbusReply::finished, this,
                [this, reply]() { onModbusReplyReady(reply); });
    }
}

void Plc21Device::onModbusReplyReady(QModbusReply* reply) {
    if (!reply || !m_parser) {
        if (reply)
            reply->deleteLater();
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

void Plc21Device::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::Plc21DataType) {
        auto const* dataMsg = static_cast<const Plc21DataMessage*>(&message);
        mergePartialData(dataMsg->data());
    }
}

void Plc21Device::mergePartialData(const Plc21PanelData& partialData) {
    // We received valid data - device is connected and communicating
    setConnectionState(true);
    resetCommunicationWatchdog();

    auto currentData = data();
    auto newData = std::make_shared<Plc21PanelData>(*currentData);

    bool dataChanged = false;

    // Merge digital inputs (always update if they're part of the partial data)
    // Since the parser sets default values, we need a smarter merge strategy
    // For now, we'll assume any non-default value in partialData should be merged

    // Digital inputs - always merge (parser provides full set)
    if (partialData.armGunSW != currentData->armGunSW ||
        partialData.loadAmmunitionSW != currentData->loadAmmunitionSW ||
        partialData.enableStationSW != currentData->enableStationSW ||
        partialData.homePositionSW != currentData->homePositionSW ||
        partialData.enableStabilizationSW != currentData->enableStabilizationSW ||
        partialData.authorizeSw != currentData->authorizeSw ||
        partialData.switchCameraSW != currentData->switchCameraSW ||
        partialData.menuUpSW != currentData->menuUpSW ||
        partialData.menuDownSW != currentData->menuDownSW ||
        partialData.menuValSw != currentData->menuValSw) {

        newData->armGunSW = partialData.armGunSW;
        newData->loadAmmunitionSW = partialData.loadAmmunitionSW;
        newData->enableStationSW = partialData.enableStationSW;
        newData->homePositionSW = partialData.homePositionSW;
        newData->enableStabilizationSW = partialData.enableStabilizationSW;
        newData->authorizeSw = partialData.authorizeSw;
        newData->switchCameraSW = partialData.switchCameraSW;
        newData->menuUpSW = partialData.menuUpSW;
        newData->menuDownSW = partialData.menuDownSW;
        newData->menuValSw = partialData.menuValSw;
        dataChanged = true;
    }

    // Analog inputs - merge if different
    if (partialData.speedSW != currentData->speedSW ||
        partialData.fireMode != currentData->fireMode ||
        partialData.panelTemperature != currentData->panelTemperature) {

        newData->speedSW = partialData.speedSW;
        newData->fireMode = partialData.fireMode;
        newData->panelTemperature = partialData.panelTemperature;
        dataChanged = true;
    }

    if (dataChanged) {
        updateData(newData);
        emit panelDataChanged(*newData);
    }
}

//================================================================================
// PUBLIC API - OUTPUT CONTROL
//================================================================================

void Plc21Device::setDigitalOutputs(const QVector<bool>& outputs) {
    m_digitalOutputs = outputs;
    sendWriteRequest(Plc21Registers::DIGITAL_OUTPUTS_START_ADDR, outputs);
}

void Plc21Device::setGunArmedLed(bool on) {
    writeDigitalOutput(0, on);
}

void Plc21Device::setStationEnabledLed(bool on) {
    writeDigitalOutput(1, on);
}

void Plc21Device::sethatchStateLed(bool on) {
    writeDigitalOutput(2, on);
}

void Plc21Device::setPanelBacklight(bool on) {
    writeDigitalOutput(3, on);
}

void Plc21Device::writeDigitalOutput(int index, bool value) {
    if (index < 0 || index >= Plc21Registers::DIGITAL_OUTPUTS_COUNT) {
        qWarning() << m_identifier << "Invalid output index:" << index;
        return;
    }

    // Ensure vector has enough elements
    while (m_digitalOutputs.size() <= index) {
        m_digitalOutputs.append(false);
    }

    m_digitalOutputs[index] = value;
    sendWriteRequest(Plc21Registers::DIGITAL_OUTPUTS_START_ADDR, m_digitalOutputs);
}

void Plc21Device::sendWriteRequest(int startAddress, const QVector<bool>& values) {
    if (state() != DeviceState::Online || !m_transport)
        return;

    QModbusDataUnit writeUnit(QModbusDataUnit::Coils, startAddress, values.size());
    for (int i = 0; i < values.size(); ++i) {
        writeUnit.setValue(i, values[i] ? 1 : 0);
    }

    QModbusReply* reply = nullptr;
    QMetaObject::invokeMethod(m_transport, "sendWriteRequest", Qt::DirectConnection,
                              Q_RETURN_ARG(QModbusReply*, reply),
                              Q_ARG(QModbusDataUnit, writeUnit));

    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            bool success = (reply->error() == QModbusDevice::NoError);
            if (!success) {
                //qWarning() << m_identifier << "Write error:" << reply->errorString();
            }
            emit digitalOutputWritten(success);
            reply->deleteLater();
        });
    }
}

void Plc21Device::setPollInterval(int intervalMs) {
    m_pollTimer->setInterval(intervalMs);
}

void Plc21Device::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void Plc21Device::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<Plc21PanelData>(*currentData);
        newData->isConnected = connected;
        updateData(newData);
        emit panelDataChanged(*newData);

        if (connected) {
            qDebug() << m_identifier << "connected";
        } else {
            qWarning() << m_identifier << "disconnected";
        }
    }
}

void Plc21Device::sendNextPendingRequest() {
    // If we need to read holding registers, send that request now
    if (m_needsHoldingRegistersRead) {
        m_needsHoldingRegistersRead = false;
        m_waitingForResponse = true;
        sendReadRequest(Plc21Registers::ANALOG_INPUTS_START_ADDR,
                        Plc21Registers::ANALOG_INPUTS_COUNT, false);
    } else {
        // Poll cycle complete - mark as inactive and schedule next cycle
        m_pollCycleActive = false;

        // Start timer for next poll cycle (adaptive polling)
        // Timer will fire after the configured interval
        m_pollTimer->start();
    }
}

void Plc21Device::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
    //           << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

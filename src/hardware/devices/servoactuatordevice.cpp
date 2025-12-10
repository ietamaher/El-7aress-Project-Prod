#include "hardware/devices/servoactuatordevice.h"
#include "hardware/interfaces/Transport.h"
#include "hardware/protocols/ServoActuatorProtocolParser.h"
#include "hardware/messages/ServoActuatorMessage.h"
#include <QDebug>

ServoActuatorDevice::ServoActuatorDevice(const QString& identifier, QObject* parent)
    : TemplatedDevice<ServoActuatorData>(parent),
      m_identifier(identifier),
      m_commandTimeoutTimer(new QTimer(this)),
      m_statusCheckTimer(new QTimer(this)),
      m_communicationWatchdog(new QTimer(this))
{
    m_commandTimeoutTimer->setSingleShot(true);
    connect(m_commandTimeoutTimer, &QTimer::timeout,
            this, &ServoActuatorDevice::handleCommandTimeout);

    connect(m_statusCheckTimer, &QTimer::timeout,
            this, &ServoActuatorDevice::checkActuatorStatus);

    m_communicationWatchdog->setSingleShot(true);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout,
            this, &ServoActuatorDevice::onCommunicationWatchdogTimeout);
}

ServoActuatorDevice::~ServoActuatorDevice() {
    m_commandTimeoutTimer->stop();
    m_statusCheckTimer->stop();
    m_communicationWatchdog->stop();
}

void ServoActuatorDevice::setDependencies(Transport* transport,
                                           ServoActuatorProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    // Parent them to this device for lifetime management
    m_transport->setParent(this);
    m_parser->setParent(this);

    // Connect transport signals
    connect(m_transport, &Transport::frameReceived,
            this, &ServoActuatorDevice::onFrameReceived);

    // Handle transport disconnect (but not connect - we only show connected when we receive valid data)
    connect(m_transport, &Transport::connectionStateChanged,
            this, [this](bool connected) {
        if (!connected) {
            onTransportDisconnected();
        }
    });
}

bool ServoActuatorDevice::initialize() {
    setState(DeviceState::Initializing);

    if (!m_transport || !m_parser) {
        qCritical() << m_identifier << "missing dependencies!";
        setState(DeviceState::Error);
        return false;
    }

    // Transport should already be opened by SystemController
    qDebug() << m_identifier << "initialized successfully";

    setState(DeviceState::Online);
    m_statusCheckTimer->start(STATUS_CHECK_INTERVAL_MS);

    // Start communication watchdog - will fire if we don't receive data
    m_communicationWatchdog->start();

    // Emit initial disconnected state (device starts disconnected until proven otherwise)
    auto initialData = data();
    if (!initialData->isConnected) {
        emit actuatorDataChanged(*initialData);
        qDebug() << m_identifier << "Initial state: disconnected (waiting for communication)";
    }

    return true;
}

void ServoActuatorDevice::shutdown() {
    m_commandTimeoutTimer->stop();
    m_statusCheckTimer->stop();
    m_communicationWatchdog->stop();
    m_commandQueue.clear();
    m_pendingCommand.clear();
    if (m_parser) {
        m_parser->setPendingCommand("");  // Clear parser's pending command
    }

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    setConnectionState(false);
    setState(DeviceState::Offline);
}

void ServoActuatorDevice::onFrameReceived(const QByteArray& frame) {
    if (!m_parser) return;
    
    // Parse frame into messages
    auto messages = m_parser->parse(frame);
    
    // Process each message
    for (const auto& msg : messages) {
        if (msg) {
            processMessage(*msg);
        }
    }
}

void ServoActuatorDevice::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::ServoActuatorDataType) {
        auto const* dataMsg = static_cast<const ServoActuatorDataMessage*>(&message);
        qDebug() << "[ServoActuator][RX] Received position data: pos=" << dataMsg->data().position_mm;
        mergePartialData(dataMsg->data());
        
    } else if (message.typeId() == Message::Type::ServoActuatorAckType) {
        auto const* ackMsg = static_cast<const ServoActuatorAckMessage*>(&message);

        qDebug() << "[ServoActuator][ACK] Command '" << ackMsg->command() << "' ACKed. Queue size:" << m_commandQueue.size();

        // Stop timeout timer
        m_commandTimeoutTimer->stop();
        m_pendingCommand.clear();
        m_parser->setPendingCommand("");  // Clear parser's pending command

        emit commandAcknowledged(ackMsg->command());

        // Process next command if any
        if (!m_commandQueue.isEmpty()) {
            qDebug() << "[ServoActuator][ACK] Processing next command in" << INTER_COMMAND_DELAY_MS << "ms";
            QTimer::singleShot(INTER_COMMAND_DELAY_MS, this,
                               &ServoActuatorDevice::processNextCommand);
        } else {
            qDebug() << "[ServoActuator][ACK] Queue empty, no more commands";
        }
        
    } else if (message.typeId() == Message::Type::ServoActuatorNackType) {
        auto const* nackMsg = static_cast<const ServoActuatorNackMessage*>(&message);

        m_commandTimeoutTimer->stop();
        m_pendingCommand.clear();
        m_parser->setPendingCommand("");  // Clear parser's pending command

        emit commandError(QString("Command '%1' rejected: %2")
                          .arg(nackMsg->command(), nackMsg->errorDetails()));

        // Process next command if any
        if (!m_commandQueue.isEmpty()) {
            QTimer::singleShot(INTER_COMMAND_DELAY_MS, this,
                               &ServoActuatorDevice::processNextCommand);
        }
        
    } else if (message.typeId() == Message::Type::ServoActuatorCriticalFaultType) {
        auto const* faultMsg = static_cast<const ServoActuatorCriticalFaultMessage*>(&message);
        emit criticalFaultOccurred(faultMsg->criticalFaults());
    }
}

void ServoActuatorDevice::mergePartialData(const ServoActuatorData& partialData) {
    auto currentData = data();
    auto newData = std::make_shared<ServoActuatorData>(*currentData);

    // We received valid data - device is connected and communicating
    setConnectionState(true);
    resetCommunicationWatchdog();

    bool dataChanged = false;

    // CRITICAL FIX: Compare with current value, NOT zero!
    // Zero is a VALID value (0.0mm = home position, 0.0mm/s = stopped, 0.0°C = valid temp)
    // Using +1.0 offset for qFuzzyCompare to handle zero values correctly
    if (!qFuzzyCompare(partialData.position_mm + 1.0, currentData->position_mm + 1.0)) {
        newData->position_mm = partialData.position_mm;
        dataChanged = true;
    }

    if (!qFuzzyCompare(partialData.velocity_mm_s + 1.0, currentData->velocity_mm_s + 1.0)) {
        newData->velocity_mm_s = partialData.velocity_mm_s;
        dataChanged = true;
    }

    if (!qFuzzyCompare(partialData.temperature_c + 1.0, currentData->temperature_c + 1.0)) {
        newData->temperature_c = partialData.temperature_c;
        dataChanged = true;
    }

    if (!qFuzzyCompare(partialData.busVoltage_v + 1.0, currentData->busVoltage_v + 1.0)) {
        newData->busVoltage_v = partialData.busVoltage_v;
        dataChanged = true;
    }

    if (!qFuzzyCompare(partialData.torque_percent + 1.0, currentData->torque_percent + 1.0)) {
        newData->torque_percent = partialData.torque_percent;
        dataChanged = true;
    }

    if (partialData.status != currentData->status) {
        newData->status = partialData.status;
        dataChanged = true;
    }

    if (dataChanged) {
        updateData(newData);
        qDebug() << "[ServoActuator][SIGNAL] EMITTING actuatorDataChanged! pos=" << newData->position_mm
                 << "vel=" << newData->velocity_mm_s
                 << "isConnected=" << newData->isConnected;
        emit actuatorDataChanged(*newData);
    } else {
        // DEBUG: Log when data hasn't changed (signal NOT emitted)
        qDebug() << "[ServoActuator][NO_CHANGE] Data unchanged, signal NOT emitted. Current pos="
                 << currentData->position_mm;
    }
}

void ServoActuatorDevice::sendCommand(const QString& command) {
    if (state() != DeviceState::Online || !m_transport || !m_parser) {
        qWarning() << m_identifier << "Cannot send command: device not ready";
        return;
    }

    // If a command is already pending, queue this one
    if (!m_pendingCommand.isEmpty()) {
        qDebug() << "[ServoActuator][CMD] Command '" << command << "' QUEUED (pending:" << m_pendingCommand << ")";
        m_commandQueue.enqueue(command);
        return;
    }

    // Build command with checksum using parser
    QByteArray fullCommand = m_parser->buildCommand(command);

    m_pendingCommand = command;
    m_parser->setPendingCommand(command);  // Set pending command in parser for response routing
    qDebug() << "[ServoActuator][CMD] SENDING command '" << command << "' (timeout=" << COMMAND_TIMEOUT_MS << "ms)";
    m_transport->sendFrame(fullCommand);
    m_commandTimeoutTimer->start(COMMAND_TIMEOUT_MS);
}

void ServoActuatorDevice::processNextCommand() {
    if (!m_commandQueue.isEmpty() && m_pendingCommand.isEmpty()) {
        sendCommand(m_commandQueue.dequeue());
    }
}

void ServoActuatorDevice::handleCommandTimeout() {
    //qWarning() << m_identifier << "Timeout waiting for response to:" << m_pendingCommand;
    emit commandError(QString("Timeout on command: %1").arg(m_pendingCommand));

    m_pendingCommand.clear();
    m_parser->setPendingCommand("");  // Clear parser's pending command

    // Try next command if any
    if (!m_commandQueue.isEmpty()) {
        QTimer::singleShot(INTER_COMMAND_DELAY_MS, this,
                           &ServoActuatorDevice::processNextCommand);
    }
}

//================================================================================
// PUBLIC API - MOTION CONTROL
//================================================================================

void ServoActuatorDevice::moveToPosition(double position_mm) {
    // Use parser for unit conversion (mm → counts for hardware command)
    int counts = m_parser->millimetersToSensorCounts(position_mm);
    qDebug() << "[ServoActuator][MOVE] moveToPosition(" << position_mm << "mm) → TA" << counts << "counts";
    sendCommand(QString("TA%1").arg(counts));
}

void ServoActuatorDevice::setMaxSpeed(double speed_mm_s) {
    int counts = m_parser->speedToSensorCounts(speed_mm_s);
    sendCommand(QString("SP%1").arg(counts));
}

void ServoActuatorDevice::setAcceleration(double accel_mm_s2) {
    int counts = m_parser->accelToSensorCounts(accel_mm_s2);
    sendCommand(QString("AC%1").arg(counts));
}

void ServoActuatorDevice::setMaxTorque(double percent) {
    int counts = m_parser->torquePercentToSensorCounts(percent);
    sendCommand(QString("MT%1").arg(counts));
}

void ServoActuatorDevice::stopMove() {
    sendCommand("TK");
}

void ServoActuatorDevice::holdCurrentPosition() {
    sendCommand("PC");
}

//================================================================================
// PUBLIC API - STATUS & DIAGNOSTICS
//================================================================================

void ServoActuatorDevice::checkAllStatus() {
    // Queue up all polling commands
    //m_commandQueue.enqueue("SR");  // Status register
    m_commandQueue.enqueue("AP");  // Position
    //m_commandQueue.enqueue("VL");  // Velocity
    //m_commandQueue.enqueue("TQ");  // Torque
    //m_commandQueue.enqueue("RT1"); // Temperature
    //m_commandQueue.enqueue("BV");  // Bus voltage

    qDebug() << "[ServoActuator][POLL] AP queued. PendingCmd:" << m_pendingCommand
             << "QueueSize after:" << m_commandQueue.size();

    // Start processing if no command is pending
    if (m_pendingCommand.isEmpty()) {
        qDebug() << "[ServoActuator][POLL] No pending command, processing next immediately";
        processNextCommand();
    } else {
        qDebug() << "[ServoActuator][POLL] Command pending (" << m_pendingCommand
                 << "), AP poll will wait in queue";
    }
}

void ServoActuatorDevice::checkStatusRegister() {
    sendCommand("SR");
}

void ServoActuatorDevice::checkPosition() {
    sendCommand("AP");
}

void ServoActuatorDevice::checkVelocity() {
    sendCommand("VL");
}

void ServoActuatorDevice::checkTorque() {
    sendCommand("TQ");
}

void ServoActuatorDevice::checkTemperature() {
    sendCommand("RT1");
}

void ServoActuatorDevice::checkBusVoltage() {
    sendCommand("BV");
}

//================================================================================
// PUBLIC API - SYSTEM & CONFIGURATION
//================================================================================

void ServoActuatorDevice::saveSettings() {
    sendCommand("CW321");
}

void ServoActuatorDevice::clearFaults() {
    sendCommand("ZF");
}

void ServoActuatorDevice::reboot() {
    sendCommand("ZR321");
}

void ServoActuatorDevice::checkActuatorStatus() {
    // Periodically query all status parameters
    qDebug() << "[ServoActuator][POLL] checkActuatorStatus timer fired. PendingCmd:" << m_pendingCommand
             << "QueueSize:" << m_commandQueue.size();
    checkAllStatus();
}

//================================================================================
// PRIVATE - CONNECTION STATE MANAGEMENT
//================================================================================

void ServoActuatorDevice::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<ServoActuatorData>(*currentData);
        newData->isConnected = connected;
        updateData(newData);
        emit actuatorDataChanged(*newData);

        if (connected) {
            qDebug() << m_identifier << "Communication established";
        } else {
            qWarning() << m_identifier << "Communication lost";
        }
    }
}

void ServoActuatorDevice::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void ServoActuatorDevice::onTransportDisconnected() {
    qWarning() << m_identifier << "Transport disconnected";
    m_communicationWatchdog->stop();
    setConnectionState(false);
}

void ServoActuatorDevice::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
   //            << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

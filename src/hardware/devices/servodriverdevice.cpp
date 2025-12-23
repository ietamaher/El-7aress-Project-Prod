#include "servodriverdevice.h"
#include "../interfaces/Transport.h"
#include "../protocols/ServoDriverProtocolParser.h"
#include "../messages/ServoDriverMessage.h"
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>

ServoDriverDevice::ServoDriverDevice(const QString& identifier, QObject* parent)
    : TemplatedDevice<ServoDriverData>(parent),
      m_identifier(identifier),
      m_pollTimer(new QTimer(this)),
      m_temperatureTimer(new QTimer(this)),
      m_communicationWatchdog(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &ServoDriverDevice::pollTimerTimeout);
    connect(m_temperatureTimer, &QTimer::timeout, this, &ServoDriverDevice::temperatureTimerTimeout);

    m_communicationWatchdog->setSingleShot(true);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout,
            this, &ServoDriverDevice::onCommunicationWatchdogTimeout);
}

ServoDriverDevice::~ServoDriverDevice() {
    m_pollTimer->stop();
    m_temperatureTimer->stop();
    m_communicationWatchdog->stop();

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    setState(DeviceState::Offline);
}

void ServoDriverDevice::setDependencies(Transport* transport,
                                         ServoDriverProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    // Parent them to this device for lifetime management
    m_transport->setParent(this);
    m_parser->setParent(this);

    // Don't listen to transport connectionStateChanged - we manage connection via watchdog
    // This prevents spurious disconnection warnings during transport initialization
    // when QModbusClient goes through intermediate states (Connecting, etc.)
}

bool ServoDriverDevice::initialize() {
    setState(DeviceState::Initializing);

    if (!m_transport || !m_parser) {
        qCritical() << m_identifier << "missing dependencies!";
        setState(DeviceState::Error);
        return false;
    }

    // Transport should already be opened by SystemController
    qDebug() << m_identifier << "initializing...";

    // Get polling intervals from config (defaults: 50ms poll, 5s temperature)
    QJsonObject config = property("config").toJsonObject();
    int pollInterval = config["pollIntervalMs"].toInt(50);
    int tempInterval = config["temperatureIntervalMs"].toInt(2000);

    setState(DeviceState::Online);

    // Start communication watchdog to detect connection issues
    m_communicationWatchdog->start();

    // Start polling
    m_pollTimer->start(pollInterval);
    m_temperatureTimer->setInterval(tempInterval);
    if (m_temperatureEnabled) {
        // Start temperature timer with offset to avoid collision with position poll
        // Delay by half the poll interval to stagger the reads
        QTimer::singleShot(pollInterval / 2, this, [this]() {
            m_temperatureTimer->start();
        });
    }

    qDebug() << m_identifier << "initialized successfully with poll interval:" << pollInterval << "ms";
    return true;
}

void ServoDriverDevice::shutdown() {
    m_pollTimer->stop();
    m_temperatureTimer->stop();
    m_communicationWatchdog->stop();

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    setConnectionState(false);
    setState(DeviceState::Offline);
}

void ServoDriverDevice::pollTimerTimeout() {
    // Read position data every poll cycle
    sendReadRequest(ServoDriverRegisters::POSITION_START_ADDR, 
                    ServoDriverRegisters::POSITION_REG_COUNT);
}

void ServoDriverDevice::temperatureTimerTimeout() {
    // Read temperature data periodically
    sendReadRequest(ServoDriverRegisters::TEMPERATURE_START_ADDR, 
                    ServoDriverRegisters::TEMPERATURE_REG_COUNT);
}

void ServoDriverDevice::sendReadRequest(int startAddress, int count) {
    if (state() != DeviceState::Online || !m_transport) return;

    // Cast to ModbusTransport to access Modbus-specific methods
    auto modbusTransport = qobject_cast<QModbusRtuSerialClient*>(
        m_transport->property("client").value<QObject*>());
    
    if (!modbusTransport) return;

    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, startAddress, count);
    
    // Send request via transport (transport handles the Modbus details)
    // Note: In real implementation, ModbusTransport would expose sendReadRequest method
    // For now, we'll use QMetaObject::invokeMethod for loose coupling
    
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

void ServoDriverDevice::onModbusReplyReady(QModbusReply* reply) {
    if (!reply || !m_parser) {
        if (reply) reply->deleteLater();
        return;
    }

    if (reply->error() != QModbusDevice::NoError) {
        //qWarning() << m_identifier << "Modbus error:" << reply->errorString();
        setConnectionState(false);  // Only emits if state actually changes
        reply->deleteLater();
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
}

void ServoDriverDevice::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::ServoDriverDataType) {
        auto const* dataMsg = static_cast<const ServoDriverDataMessage*>(&message);

        // We received valid data - device is connected and communicating
        setConnectionState(true);
        resetCommunicationWatchdog();

        // Merge partial data with current data
        auto currentData = data();
        auto newData = std::make_shared<ServoDriverData>(*currentData);

        const ServoDriverData& partialData = dataMsg->data();

        bool dataChanged = false;

        // Update fields that have changed (comparing with current value, NOT zero!)
        // CRITICAL FIX: Zero is a VALID value (0.0° = home position, 0.0°C = valid temp)
        if (!qFuzzyCompare(partialData.position + 1.0f, currentData->position + 1.0f)) {
            newData->position = partialData.position;
            dataChanged = true;
        }
        if (!qFuzzyCompare(partialData.rpm + 1.0f, currentData->rpm + 1.0f)) {
            newData->rpm = partialData.rpm;
            dataChanged = true;
        }

        if (!qFuzzyCompare(partialData.driverTemp + 1.0f, currentData->driverTemp + 1.0f)) {
            newData->driverTemp = partialData.driverTemp;
            dataChanged = true;
        }
        if (!qFuzzyCompare(partialData.motorTemp + 1.0f, currentData->motorTemp + 1.0f)) {
            newData->motorTemp = partialData.motorTemp;
            dataChanged = true;
        }

        if (dataChanged) {
            updateData(newData);
            emit servoDataChanged(*newData);  // Only emit when data actually changed
        }
        
    } else if (message.typeId() == Message::Type::ServoDriverAlarmType) {
        auto const* alarmMsg = static_cast<const ServoDriverAlarmMessage*>(&message);

        // Update alarm status - alarm code is emitted via signal, not stored in data
        auto newData = std::make_shared<ServoDriverData>(*data());
        newData->fault = true;
        updateData(newData);

        emit alarmDetected(alarmMsg->alarmCode(), alarmMsg->description());
        emit servoDataChanged(*newData);
        
    } else if (message.typeId() == Message::Type::ServoDriverAlarmHistoryType) {
        auto const* historyMsg = static_cast<const ServoDriverAlarmHistoryMessage*>(&message);
        emit alarmHistoryRead(historyMsg->alarmHistory());
    }
}

//================================================================================
// PUBLIC API - COMMAND INTERFACE
//================================================================================

void ServoDriverDevice::writePosition(float position) {
    int32_t positionRaw = static_cast<int32_t>(position);
    quint16 highWord = (positionRaw >> 16) & 0xFFFF;
    quint16 lowWord = positionRaw & 0xFFFF;
    
    sendWriteRequest(ServoDriverRegisters::POSITION_START_ADDR, 
                     QVector<quint16>{highWord, lowWord});
}

void ServoDriverDevice::writeSpeed(float speed) {
    int32_t speedRaw = static_cast<int32_t>(speed);
    quint16 highWord = (speedRaw >> 16) & 0xFFFF;
    quint16 lowWord = speedRaw & 0xFFFF;
    
    // Note: Define SPEED_START_ADDR in ServoDriverRegisters namespace
    // sendWriteRequest(ServoDriverRegisters::SPEED_START_ADDR, 
    //                  QVector<quint16>{highWord, lowWord});
}

void ServoDriverDevice::writeAcceleration(float accel) {
    // Similar to writeSpeed
}

void ServoDriverDevice::writeTorqueLimit(float torque) {
    // Similar to writePosition
}

void ServoDriverDevice::writeData(int startAddress, const QVector<quint16>& values) {
    sendWriteRequest(startAddress, values);
}

void ServoDriverDevice::readAlarmStatus() {
    sendReadRequest(ServoDriverRegisters::ALARM_STATUS_ADDR,
                    ServoDriverRegisters::ALARM_STATUS_REG_COUNT);
}

void ServoDriverDevice::clearAlarm() {
    sendWriteRequest(ServoDriverRegisters::ALARM_RESET_ADDR,
                     QVector<quint16>{0, 1}); // Execute clear command
    
    // Reset register back to 0
    QTimer::singleShot(100, this, [this]() {
        sendWriteRequest(ServoDriverRegisters::ALARM_RESET_ADDR,
                         QVector<quint16>{0, 0});
        
        auto newData = std::make_shared<ServoDriverData>(*data());
        newData->fault = false;
        //newData->alarmCode = 0;
        updateData(newData);
        emit alarmCleared();
        emit servoDataChanged(*newData);
    });
}

void ServoDriverDevice::readAlarmHistory() {
    sendReadRequest(ServoDriverRegisters::ALARM_HISTORY_ADDR,
                    ServoDriverRegisters::ALARM_HISTORY_REG_COUNT);
}

void ServoDriverDevice::clearAlarmHistory() {
    sendWriteRequest(ServoDriverRegisters::ALARM_HISTORY_CLEAR_ADDR,
                     QVector<quint16>{0, 1});
    
    QTimer::singleShot(100, this, [this]() {
        sendWriteRequest(ServoDriverRegisters::ALARM_HISTORY_CLEAR_ADDR,
                         QVector<quint16>{0, 0});
    });
}

void ServoDriverDevice::enableTemperatureReading(bool enable) {
    m_temperatureEnabled = enable;
    if (enable && state() == DeviceState::Online) {
        m_temperatureTimer->start();
    } else {
        m_temperatureTimer->stop();
    }
}

void ServoDriverDevice::setTemperatureInterval(int intervalMs) {
    m_temperatureTimer->setInterval(intervalMs);
}

void ServoDriverDevice::sendWriteRequest(int startAddress, const QVector<quint16>& values) {
    if (state() != DeviceState::Online || !m_transport) return;

    // ✅ LATENCY PROFILING: Start timing the Modbus write operation
    m_modbusWriteTimer.start();

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, values);

    QModbusReply* reply = nullptr;
    QMetaObject::invokeMethod(m_transport, "sendWriteRequest",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(QModbusReply*, reply),
                              Q_ARG(QModbusDataUnit, writeUnit));

    // ✅ LATENCY PROFILING: Measure how long the write took (nanosecond precision)
    qint64 elapsedNs = m_modbusWriteTimer.nsecsElapsed();
    m_modbusWriteCount++;
    m_modbusWriteTotalNs += elapsedNs;
    m_modbusWriteMaxNs = qMax(m_modbusWriteMaxNs, elapsedNs);
    m_modbusWriteMinNs = qMin(m_modbusWriteMinNs, elapsedNs);

    // Log statistics every 100 writes to detect event queue blocking
    if (m_modbusWriteCount % 100 == 0) {
        double avgMs = (m_modbusWriteTotalNs / (double)m_modbusWriteCount) / 1000000.0;
        double maxMs = m_modbusWriteMaxNs / 1000000.0;
        double minMs = m_modbusWriteMinNs / 1000000.0;
        /*qDebug() << "⚙️ [MODBUS WRITE]" << m_identifier << "100 writes |"
                 << "Avg:" << QString::number(avgMs, 'f', 3) << "ms"
                 << "Min:" << QString::number(minMs, 'f', 3) << "ms"
                 << "Max:" << QString::number(maxMs, 'f', 3) << "ms"
                 << "| Total:" << m_modbusWriteCount;*/

        // Reset for next batch
        m_modbusWriteTotalNs = 0;
        m_modbusWriteMaxNs = 0;
        m_modbusWriteMinNs = 999999999;
        m_modbusWriteCount = 0;
    }

    if (reply) {
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    }
}

//================================================================================
// PRIVATE - CONNECTION STATE MANAGEMENT
//================================================================================

void ServoDriverDevice::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<ServoDriverData>(*currentData);
        newData->isConnected = connected;
        updateData(newData);
        emit servoDataChanged(*newData);

        if (connected) {
            qDebug() << m_identifier << "Communication established";
        } else {
            qWarning() << m_identifier << "Communication lost";
        }
    }
}

void ServoDriverDevice::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void ServoDriverDevice::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
    //           << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

#include "imudevice.h"
#include "../interfaces/Transport.h"
#include "../protocols/Imu3DMGX3ProtocolParser.h"
#include "../messages/ImuMessage.h"
#include <QDebug>

ImuDevice::ImuDevice(const QString& identifier, QObject* parent)
    : TemplatedDevice<ImuData>(parent),
      m_identifier(identifier),
      m_pollTimer(new QTimer(this)),
      m_communicationWatchdog(new QTimer(this)),
      m_gyroBiasTimer(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &ImuDevice::pollTimerTimeout);

    // FIXED: Changed from false to true - watchdog should be single-shot
    m_communicationWatchdog->setSingleShot(true);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout,
            this, &ImuDevice::onCommunicationWatchdogTimeout);

    m_gyroBiasTimer->setSingleShot(true);
    m_gyroBiasTimer->setInterval(GYRO_BIAS_TIMEOUT_MS);
    connect(m_gyroBiasTimer, &QTimer::timeout,
            this, &ImuDevice::onGyroBiasTimeout);
}

ImuDevice::~ImuDevice() {
    m_pollTimer->stop();
    m_communicationWatchdog->stop();
    m_gyroBiasTimer->stop();
}

void ImuDevice::setDependencies(Transport* transport,
                                 Imu3DMGX3ProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    m_transport->setParent(this);
    m_parser->setParent(this);

    // Connect to transport's frameReceived signal
    connect(m_transport, &Transport::frameReceived,
            this, &ImuDevice::processFrame);
}

bool ImuDevice::initialize() {
    setState(DeviceState::Initializing);

    if (!m_transport || !m_parser) {
        qCritical() << m_identifier << "missing dependencies!";
        setState(DeviceState::Error);
        return false;
    }

    qDebug() << m_identifier << "initializing 3DM-GX3-25...";

    // Get poll interval from config (default 40ms = 25Hz)
    QJsonObject config = property("config").toJsonObject();
    int samplingRateHz = config["samplingRateHz"].toInt(25);
    m_pollIntervalMs = 1000 / samplingRateHz;
    qDebug() << m_identifier << "Config received: samplingRateHz =" << samplingRateHz
             << "Hz, pollInterval =" << m_pollIntervalMs << "ms";

    // Step 1: Capture gyro bias (device must be stationary!)
    qWarning() << m_identifier << "**IMPORTANT**: Device must be stationary for gyro bias capture!";
    captureGyroBias();

    return true;
}

void ImuDevice::captureGyroBias() {
    qDebug() << m_identifier << "Capturing gyro bias (10 seconds)...";
    
    m_waitingForGyroBias = true;

    // Send 0xCD command: [0xCD, 0xC1, 0x29, TimeH, TimeL]
    QByteArray cmd = Imu3DMGX3ProtocolParser::createCaptureGyroBiasCommand(10000);
    m_transport->sendFrame(cmd);

    // Start timeout timer
    m_gyroBiasTimer->start();
}

void ImuDevice::onGyroBiasTimeout() {
    if (m_waitingForGyroBias) {
        qWarning() << m_identifier << "Gyro bias capture timed out - proceeding anyway";
        m_waitingForGyroBias = false;
        startPolling();
    }
}

void ImuDevice::startPolling() {
    qDebug() << m_identifier << "Starting polling mode at" << (1000.0 / m_pollIntervalMs) << "Hz";
    
    setState(DeviceState::Online);

    // Start polling timer and watchdog
    m_pollTimer->start(m_pollIntervalMs);
    m_communicationWatchdog->start();

    qDebug() << m_identifier << "initialized successfully!";
}

void ImuDevice::shutdown() {
    m_pollTimer->stop();
    m_communicationWatchdog->stop();
    m_gyroBiasTimer->stop();

    if (m_transport) {
        m_transport->close();
    }

    setState(DeviceState::Offline);
}

void ImuDevice::pollTimerTimeout() {
    // Send single 0xCF query
    sendReadRequest();
}

void ImuDevice::sendReadRequest() {
    if (state() != DeviceState::Online || !m_transport) return;

    // Send 0xCF command (single-shot query for Euler angles + rates)
    QByteArray cmd;
    cmd.append(static_cast<char>(0xCF));  // Single byte command
    
    m_transport->sendFrame(cmd);
}

void ImuDevice::processFrame(const QByteArray& frame) {
    if (!m_parser) return;

    // Parse the response
    auto messages = m_parser->parse(frame);

    // Check if we're waiting for gyro bias response
    if (m_waitingForGyroBias) {
        // Parser will log gyro bias values when it receives 0xCD response
        // Just check if we got any response (0xCD packet will be parsed)
        if (!messages.empty() || frame.size() >= 19) {
            // Gyro bias captured (0xCD response is 19 bytes)
            qDebug() << m_identifier << "Gyro bias capture completed";
            m_waitingForGyroBias = false;
            m_gyroBiasTimer->stop();
            startPolling();
            return;  // Don't process as normal data
        }
    }

    // Process normal data messages (0xCF responses)
    for (const auto& msg : messages) {
        if (msg) {
            processMessage(*msg);
        }
    }
}

void ImuDevice::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::ImuDataType) {
        auto const* dataMsg = static_cast<const ImuDataMessage*>(&message);

        // We received valid data - device is connected
        setConnectionState(true);
        resetCommunicationWatchdog();

        // Check if data actually changed before emitting signal
        auto currentData = data();
        auto newData = std::make_shared<ImuData>(dataMsg->data());

        // Compare with threshold (IMU data changes frequently, so emit more often)
        // But still avoid emitting identical data (reduces QML signal overhead)
        bool dataChanged = (
            std::abs(newData->rollDeg - currentData->rollDeg) > 0.01 ||
            std::abs(newData->pitchDeg - currentData->pitchDeg) > 0.01 ||
            std::abs(newData->yawDeg - currentData->yawDeg) > 0.01 ||
            std::abs(newData->angRateX_dps - currentData->angRateX_dps) > 0.1 ||
            std::abs(newData->angRateY_dps - currentData->angRateY_dps) > 0.1 ||
            std::abs(newData->angRateZ_dps - currentData->angRateZ_dps) > 0.1
        );

        updateData(newData);

        if (dataChanged) {
            emit imuDataChanged(*newData);
        }
    }
}

void ImuDevice::setPollInterval(int intervalMs) {
    m_pollIntervalMs = intervalMs;
    m_pollTimer->setInterval(intervalMs);
}

void ImuDevice::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void ImuDevice::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<ImuData>(*currentData);
        newData->isConnected = connected;
        updateData(newData);
        emit imuDataChanged(*newData);

        if (connected) {
            qDebug() << m_identifier << "connected";
        } else {
            qWarning() << m_identifier << "disconnected";
        }
    }
}

void ImuDevice::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
     //          << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

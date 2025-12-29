#include "daycameracontroldevice.h"
#include "../interfaces/Transport.h"
#include "../protocols/DayCameraProtocolParser.h"
#include "../messages/DayCameraMessage.h"
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

DayCameraControlDevice::DayCameraControlDevice(const QString& identifier, QObject* parent)
    : TemplatedDevice<DayCameraData>(parent),
      m_identifier(identifier),
      m_statusCheckTimer(new QTimer(this)),
      m_communicationWatchdog(new QTimer(this))
{
    connect(m_statusCheckTimer, &QTimer::timeout, this, &DayCameraControlDevice::checkCameraStatus);

    m_communicationWatchdog->setSingleShot(false);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout,
            this, &DayCameraControlDevice::onCommunicationWatchdogTimeout);
}

DayCameraControlDevice::~DayCameraControlDevice() {
    shutdown();
}

void DayCameraControlDevice::setDependencies(Transport* transport, DayCameraProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    m_transport->setParent(this);
    m_parser->setParent(this);

    // Only listen to frame data, not port state
    connect(m_transport, &Transport::frameReceived, this, &DayCameraControlDevice::processFrame);
}

bool DayCameraControlDevice::initialize() {
    setState(DeviceState::Initializing);

    if (!m_transport || !m_parser) {
        qCritical() << m_identifier << "missing dependencies!";
        setState(DeviceState::Error);
        return false;
    }

    // Transport should already be opened by SystemController
    qDebug() << m_identifier << "initialized successfully";

    setState(DeviceState::Online);
    m_statusCheckTimer->start(10000);  // Send stop command every 10 seconds
    m_communicationWatchdog->start();
    getCameraStatus();
    return true;
}

void DayCameraControlDevice::shutdown() {
    m_statusCheckTimer->stop();
    m_communicationWatchdog->stop();

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    setState(DeviceState::Offline);
}

void DayCameraControlDevice::processFrame(const QByteArray& frame) {
    if (!m_parser) return;

    auto messages = m_parser->parse(frame);
    for (const auto& msg : messages) {
        if (msg) processMessage(*msg);
    }
}

void DayCameraControlDevice::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::DayCameraDataType) {
        auto const* dataMsg = static_cast<const DayCameraDataMessage*>(&message);

        // We received valid data - device is connected and communicating
        setConnectionState(true);
        resetCommunicationWatchdog();

        // Merge with current data
        auto currentData = data();
        auto newData = std::make_shared<DayCameraData>(*currentData);

        const DayCameraData& partial = dataMsg->data();

        bool dataChanged = false;

        // CRITICAL FIX: Compare with current value, NOT zero!
        // Zero is a VALID value (zoom position 0 = minimum zoom, focus 0 = minimum focus)
        if (partial.zoomPosition != currentData->zoomPosition) {
            newData->zoomPosition = partial.zoomPosition;
            newData->currentHFOV = partial.currentHFOV;
            newData->currentVFOV = partial.currentVFOV;  // 16:9 aspect ratio
            dataChanged = true;
        }
        if (partial.focusPosition != currentData->focusPosition) {
            newData->focusPosition = partial.focusPosition;
            dataChanged = true;
        }

        if (dataChanged) {
            updateData(newData);
        }
        emit dayCameraDataChanged(*newData);
    }
}

void DayCameraControlDevice::sendCommand(quint8 cmd1, quint8 cmd2, quint8 data1, quint8 data2) {
    if (state() != DeviceState::Online || !m_transport || !m_parser) return;

    QByteArray command = m_parser->buildCommand(cmd1, cmd2, data1, data2);
    m_transport->sendFrame(command);
}

void DayCameraControlDevice::zoomIn() {
    auto newData = std::make_shared<DayCameraData>(*data());
    newData->zoomMovingIn = true;
    newData->zoomMovingOut = false;
    updateData(newData);
    emit dayCameraDataChanged(*newData);
    sendCommand(0x00, 0x20);
}

void DayCameraControlDevice::zoomOut() {
    auto newData = std::make_shared<DayCameraData>(*data());
    newData->zoomMovingOut = true;
    newData->zoomMovingIn = false;
    updateData(newData);
    emit dayCameraDataChanged(*newData);
    sendCommand(0x00, 0x40);
}

void DayCameraControlDevice::zoomStop() {
    auto newData = std::make_shared<DayCameraData>(*data());
    newData->zoomMovingIn = false;
    newData->zoomMovingOut = false;
    updateData(newData);
    emit dayCameraDataChanged(*newData);
    sendCommand(0x00, 0x00);
}

void DayCameraControlDevice::setZoomPosition(quint16 position) {
    quint8 high = (position >> 8) & 0xFF;
    quint8 low = position & 0xFF;
    sendCommand(0x00, 0xA7, high, low);
}

void DayCameraControlDevice::focusNear() {
    sendCommand(0x01, 0x00);
}

void DayCameraControlDevice::focusFar() {
    sendCommand(0x00, 0x02);
}

void DayCameraControlDevice::focusStop() {
    sendCommand(0x00, 0x00);
}

void DayCameraControlDevice::setFocusAuto(bool enabled) {
    auto newData = std::make_shared<DayCameraData>(*data());
    newData->autofocusEnabled = enabled;
    updateData(newData);
    emit dayCameraDataChanged(*newData);

    sendCommand(0x01, enabled ? 0x63 : 0x64);
}

void DayCameraControlDevice::setFocusPosition(quint16 position) {
    quint8 high = (position >> 8) & 0xFF;
    quint8 low = position & 0xFF;
    sendCommand(0x00, 0x63, high, low);
}

void DayCameraControlDevice::getCameraStatus() {
    sendCommand(0x00, 0x02);
}

void DayCameraControlDevice::checkCameraStatus() {
    // Query zoom position to sync camera state
    getCameraStatus();  // Pelco-D: 0x00, 0xA7 - Get zoom position
    // Also send stop command to keep communication alive
    zoomStop();
}

void DayCameraControlDevice::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void DayCameraControlDevice::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<DayCameraData>(*currentData);
        newData->isConnected = connected;
        updateData(newData);
        emit dayCameraDataChanged(*newData);

        if (connected) {
            qDebug() << m_identifier << "connected";
        } else {
            qWarning() << m_identifier << "disconnected";
        }
    }
}

void DayCameraControlDevice::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
    //           << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

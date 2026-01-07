#include "nightcameracontroldevice.h"
#include "../interfaces/Transport.h"
#include "../protocols/NightCameraProtocolParser.h"
#include "../messages/NightCameraMessage.h"
#include <QJsonObject>
#include <QDebug>

NightCameraControlDevice::NightCameraControlDevice(const QString& identifier, QObject* parent)
    : TemplatedDevice<NightCameraData>(parent), m_identifier(identifier),
      m_statusCheckTimer(new QTimer(this)), m_communicationWatchdog(new QTimer(this)) {
    connect(m_statusCheckTimer, &QTimer::timeout, this,
            &NightCameraControlDevice::checkCameraStatus);

    m_communicationWatchdog->setSingleShot(false);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout, this,
            &NightCameraControlDevice::onCommunicationWatchdogTimeout);
}

NightCameraControlDevice::~NightCameraControlDevice() {
    shutdown();
}

void NightCameraControlDevice::setDependencies(Transport* transport,
                                               NightCameraProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    m_transport->setParent(this);
    m_parser->setParent(this);

    // Only listen to frame data, not port state
    connect(m_transport, &Transport::frameReceived, this, &NightCameraControlDevice::processFrame);
}

bool NightCameraControlDevice::initialize() {
    setState(DeviceState::Initializing);

    if (!m_transport || !m_parser) {
        qCritical() << m_identifier << "missing dependencies!";
        setState(DeviceState::Error);
        return false;
    }

    // Transport should already be opened by SystemController
    qDebug() << m_identifier << "initialized successfully";

    setState(DeviceState::Online);
    m_statusCheckTimer->start(5000);
    m_communicationWatchdog->start();
    getCameraStatus();
    return true;
}

void NightCameraControlDevice::shutdown() {
    m_statusCheckTimer->stop();
    m_communicationWatchdog->stop();

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    setState(DeviceState::Offline);
}

void NightCameraControlDevice::processFrame(const QByteArray& frame) {
    if (!m_parser)
        return;

    auto messages = m_parser->parse(frame);
    for (const auto& msg : messages) {
        if (msg)
            processMessage(*msg);
    }
}

void NightCameraControlDevice::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::NightCameraDataType) {
        auto const* dataMsg = static_cast<const NightCameraDataMessage*>(&message);

        // We received valid data - device is connected and communicating
        setConnectionState(true);
        resetCommunicationWatchdog();

        // Merge with current data
        auto currentData = data();
        auto newData = std::make_shared<NightCameraData>(*currentData);

        const NightCameraData& partial = dataMsg->data();
        newData->cameraStatus = partial.cameraStatus;
        newData->errorState = partial.errorState;
        if (!partial.ffcInProgress) {
            newData->ffcInProgress = false;
        }

        // Update temperature if received (from 0x20 READ_TEMP_SENSOR response)
        if (partial.fpaTemperature != 0) {
            newData->fpaTemperature = partial.fpaTemperature;
            //qDebug() << m_identifier << "Temperature updated:" << newData->fpaTemperature
            //         << "(" << (newData->fpaTemperature / 10.0) << "°C)";
        }

        // Update video mode/zoom if received (from 0x0F VIDEO_MODE response)
        if (partial.digitalZoomLevel != 0) {
            newData->digitalZoomEnabled = partial.digitalZoomEnabled;
            newData->digitalZoomLevel = partial.digitalZoomLevel;
            newData->currentHFOV = partial.currentHFOV;
            newData->currentVFOV = partial.currentVFOV;
            //qDebug() << m_identifier << "Zoom updated:" << newData->digitalZoomLevel << "x"
            //         << "(FOV:" << newData->currentHFOV << "×" << newData->currentVFOV << "°)";
        }

        // Update LUT if received (from 0x10 VIDEO_LUT response)
        // Note: videoMode is used for both video mode and LUT in the data structure
        if (partial.videoMode != 0) {
            newData->videoMode = partial.videoMode;
            newData->lut = static_cast<quint8>(partial.videoMode);
            //qDebug() << m_identifier << "LUT updated:" << newData->videoMode;
        }

        // Update pan/tilt if received (from 0x70 PAN_AND_TILT response)
        if (partial.panPosition != 0 || partial.tiltPosition != 0) {
            newData->panPosition = partial.panPosition;
            newData->tiltPosition = partial.tiltPosition;
            //qDebug() << m_identifier << "Pan/Tilt updated: pan=" << newData->panPosition
            //        << "tilt=" << newData->tiltPosition;
        }

        updateData(newData);
        emit nightCameraDataChanged(*newData);
    }
}

void NightCameraControlDevice::sendCommand(quint8 function, const QByteArray& cmdData) {
    if (state() != DeviceState::Online || !m_transport || !m_parser)
        return;

    QByteArray command = m_parser->buildCommand(function, cmdData);
    m_transport->sendFrame(command);
}

void NightCameraControlDevice::performFFC() {
    auto newData = std::make_shared<NightCameraData>(*data());
    newData->ffcInProgress = true;
    updateData(newData);
    emit nightCameraDataChanged(*newData);

    // 0x0C = DO_FFC (no arguments required)
    sendCommand(0x0C, QByteArray());
    qDebug() << m_identifier << "FFC commanded";
}

void NightCameraControlDevice::setDigitalZoom(quint8 zoomLevel) {
    auto newData = std::make_shared<NightCameraData>(*data());
    newData->digitalZoomEnabled = (zoomLevel > 0);
    newData->digitalZoomLevel = zoomLevel;
    newData->currentHFOV = (zoomLevel > 0) ? 5.2 : 10.4;  // FLIR TAU 2 640×512
    newData->currentVFOV = (zoomLevel > 0) ? 4.0 : 8.0;   // NOT square sensor!
    updateData(newData);
    emit nightCameraDataChanged(*newData);

    QByteArray zoomArg =
        (zoomLevel > 0) ? QByteArray::fromHex("0004") : QByteArray::fromHex("0000");
    sendCommand(0x0F, zoomArg);
}

void NightCameraControlDevice::setVideoModeLUT(quint16 mode) {
    auto newData = std::make_shared<NightCameraData>(*data());
    newData->videoMode = mode;
    updateData(newData);
    emit nightCameraDataChanged(*newData);

    if (mode > 12)
        mode = 12;
    QByteArray modeArg = QByteArray::fromHex(QByteArray::number(mode, 16).rightJustified(4, '0'));
    sendCommand(0x10, modeArg);
}

void NightCameraControlDevice::getCameraStatus() {
    sendCommand(0x06, QByteArray::fromHex("0000"));
}

void NightCameraControlDevice::checkCameraStatus() {
    getCameraStatus();
    readFpaTemperature();
    getVideoMode();
    getVideoLUT();
}

void NightCameraControlDevice::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void NightCameraControlDevice::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<NightCameraData>(*currentData);
        newData->isConnected = connected;
        updateData(newData);
        emit nightCameraDataChanged(*newData);

        if (connected) {
            qDebug() << m_identifier << "connected";
        } else {
            qWarning() << m_identifier << "disconnected";
        }
    }
}

void NightCameraControlDevice::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
    //          << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

void NightCameraControlDevice::readFpaTemperature() {
    // 0x20 READ_TEMP_SENSOR
    // Argument: 0x0000 = get temperature (TAU2 firmware returns Celsius × 10)
    // Note: Some TAU2 variants reject 0x0002 (Celsius mode) with error 0x03 "Data Out of Range"
    sendCommand(0x20, QByteArray::fromHex("0000"));
}

void NightCameraControlDevice::setPanTilt(qint16 tilt, qint16 pan) {
    // 0x70 PAN_AND_TILT
    // Clamp values to valid ranges
    tilt = qBound(qint16(-68), tilt, qint16(68));
    pan = qBound(qint16(-82), pan, qint16(82));

    auto newData = std::make_shared<NightCameraData>(*data());
    newData->tiltPosition = tilt;
    newData->panPosition = pan;
    updateData(newData);
    emit nightCameraDataChanged(*newData);

    // Build command: Bytes 0-1 = Tilt (signed), Bytes 2-3 = Pan (signed)
    QByteArray panTiltArg;
    panTiltArg.append(static_cast<char>((tilt >> 8) & 0xFF));
    panTiltArg.append(static_cast<char>(tilt & 0xFF));
    panTiltArg.append(static_cast<char>((pan >> 8) & 0xFF));
    panTiltArg.append(static_cast<char>(pan & 0xFF));

    sendCommand(0x70, panTiltArg);
    qDebug() << m_identifier << "Set pan/tilt: tilt =" << tilt << ", pan =" << pan;
}

void NightCameraControlDevice::getVideoMode() {
    // 0x0F VIDEO_MODE - Send with no argument to query current mode
    // Response: 0x0000 = Normal (1X), 0x0004 = Zoom (2X)
    sendCommand(0x0F, QByteArray());
}

void NightCameraControlDevice::getVideoLUT() {
    // 0x10 VIDEO_LUT - Send with no argument to query current LUT
    // Response: 0x0000 = White hot, 0x0001 = Black hot, etc.
    sendCommand(0x10, QByteArray());
}

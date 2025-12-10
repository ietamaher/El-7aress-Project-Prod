#include "radardevice.h"
#include "../interfaces/Transport.h"
#include "../protocols/RadarProtocolParser.h"
#include "../messages/RadarMessage.h"
#include <QJsonObject>
#include <QDebug>

RadarDevice::RadarDevice(const QString& identifier, QObject* parent)
    : TemplatedDevice<RadarData>(parent),
      m_identifier(identifier),
      m_communicationWatchdog(new QTimer(this))
{
    m_communicationWatchdog->setSingleShot(false);
    m_communicationWatchdog->setInterval(COMMUNICATION_TIMEOUT_MS);
    connect(m_communicationWatchdog, &QTimer::timeout,
            this, &RadarDevice::onCommunicationWatchdogTimeout);
}

RadarDevice::~RadarDevice() {
    m_communicationWatchdog->stop();
    shutdown();
}

void RadarDevice::setDependencies(Transport* transport,
                                   RadarProtocolParser* parser) {
    m_transport = transport;
    m_parser = parser;

    // Parent them to this device for lifetime management
    m_transport->setParent(this);
    m_parser->setParent(this);

    // Connect transport signals
    connect(m_transport, &Transport::frameReceived,
            this, &RadarDevice::processFrame);

    // Don't listen to transport connectionStateChanged - we manage connection via watchdog
}

bool RadarDevice::initialize() {
    setState(DeviceState::Initializing);

    if (!m_transport || !m_parser) {
        qCritical() << m_identifier << "missing dependencies!";
        setState(DeviceState::Error);
        return false;
    }

    // Transport should already be opened by SystemController
    qDebug() << m_identifier << "initialized successfully";

    setState(DeviceState::Online);
    m_communicationWatchdog->start();
    return true;
}

void RadarDevice::shutdown() {
    m_communicationWatchdog->stop();

    if (m_transport) {
        QMetaObject::invokeMethod(m_transport, "close", Qt::QueuedConnection);
    }

    m_trackedTargets.clear();
    setState(DeviceState::Offline);
}

void RadarDevice::processFrame(const QByteArray& frame) {
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

void RadarDevice::processMessage(const Message& message) {
    if (message.typeId() == Message::Type::RadarPlotType) {
        auto const* plotMsg = static_cast<const RadarPlotMessage*>(&message);
        const RadarData& newPlot = plotMsg->data();

        // We received valid data - device is connected and communicating
        setConnectionState(true);
        resetCommunicationWatchdog();

        // Update tracked targets
        updateTrackedTarget(newPlot);

        // Update current data (for TemplatedDevice)
        auto currentData = std::make_shared<RadarData>(newPlot);
        updateData(currentData);

        // Emit signals
        emit newPlotReceived(newPlot);
        emit radarPlotsUpdated(m_trackedTargets);
    }
}

void RadarDevice::updateTrackedTarget(const RadarData& newPlot) {
    // Find existing target by ID
    bool found = false;
    for (int i = 0; i < m_trackedTargets.size(); ++i) {
        if (m_trackedTargets[i].id == newPlot.id) {
            // Update existing target
            m_trackedTargets[i] = newPlot;
            found = true;
            break;
        }
    }

    if (!found) {
        // Add new target
        m_trackedTargets.append(newPlot);
    }
}

//================================================================================
// PUBLIC API - TARGET MANAGEMENT
//================================================================================

QVector<RadarData> RadarDevice::trackedTargets() const {
    return m_trackedTargets;
}

void RadarDevice::clearTrackedTargets() {
    m_trackedTargets.clear();
    emit radarPlotsUpdated(m_trackedTargets);
}

void RadarDevice::resetCommunicationWatchdog() {
    m_communicationWatchdog->start();
}

void RadarDevice::setConnectionState(bool connected) {
    auto currentData = data();
    if (currentData->isConnected != connected) {
        auto newData = std::make_shared<RadarData>(*currentData);
        newData->isConnected = connected;
        updateData(newData);

        if (connected) {
            qDebug() << m_identifier << "connected";
        } else {
            qWarning() << m_identifier << "disconnected";
            // Clear targets when disconnected
            m_trackedTargets.clear();
            emit radarPlotsUpdated(m_trackedTargets);
        }
    }
}

void RadarDevice::onCommunicationWatchdogTimeout() {
    //qWarning() << m_identifier << "Communication timeout - no data received for"
    //           << COMMUNICATION_TIMEOUT_MS << "ms";
    setConnectionState(false);
}

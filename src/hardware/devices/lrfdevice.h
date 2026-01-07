#ifndef LRFDEVICE_H
#define LRFDEVICE_H

#include "hardware/devices/TemplatedDevice.h"
#include "hardware/data/DataTypes.h"
#include <memory>
#include <QTimer>

class Transport;
class LrfProtocolParser;
class Message;

/**
 * @brief Laser Range Finder device implementation
 *
 * Manages communication with Jioptics LRF using dependency-injected
 * transport and protocol parser. Provides ranging, temperature reading,
 * and device status monitoring capabilities.
 */
class LRFDevice : public TemplatedDevice<LrfData> {
    Q_OBJECT

public:
    explicit LRFDevice(QObject* parent = nullptr);
    ~LRFDevice() override;

    // IDevice interface
    DeviceType type() const override;
    Q_INVOKABLE bool initialize() override;
    void shutdown() override;

    // Dependency injection
    Q_INVOKABLE void setDependencies(Transport* transport, LrfProtocolParser* parser);

    // Public API - Ranging commands
    Q_INVOKABLE void sendSelfCheck();
    Q_INVOKABLE void sendSingleRanging();
    Q_INVOKABLE void sendContinuousRanging1Hz();
    Q_INVOKABLE void sendContinuousRanging5Hz();
    Q_INVOKABLE void sendContinuousRanging10Hz();
    Q_INVOKABLE void stopRanging();

    // Public API - Status queries
    Q_INVOKABLE void queryAccumulatedLaserCount();
    Q_INVOKABLE void queryProductInfo();
    Q_INVOKABLE void queryTemperature();

signals:
    void lrfDataChanged(std::shared_ptr<const LrfData> newData);
    void productInfoReceived(quint8 productId, const QString& softwareVersion);
    void responseTimeout();

private slots:
    void processFrame(const QByteArray& frame);
    void processMessage(const Message& message);
    void handleCommandResponseTimeout();
    void checkLrfStatus();
    void onCommunicationWatchdogTimeout();

private:
    void sendCommand(quint8 commandCode);
    void resetCommunicationWatchdog();
    void setConnectionState(bool connected);

    Transport* m_transport;
    LrfProtocolParser* m_parser;
    QTimer* m_commandResponseTimer;

    QTimer* m_statusCheckTimer = nullptr;
    QTimer* m_communicationWatchdog = nullptr;

    static constexpr int COMMUNICATION_TIMEOUT_MS = 10000;  // 3 seconds without data = disconnected
};

#endif  // LRFDEVICE_H

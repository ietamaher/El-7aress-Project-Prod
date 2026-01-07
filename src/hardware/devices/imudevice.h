#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QTimer>

#include "../devices/TemplatedDevice.h"
#include "../data/DataTypes.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
class Transport;
class Imu3DMGX3ProtocolParser;
class Message;

/**
 * @brief 3DM-GX3-25 MicroStrain AHRS device (Simple polling mode)
 */
class ImuDevice : public TemplatedDevice<ImuData> {
    Q_OBJECT

public:
    explicit ImuDevice(const QString& identifier, QObject* parent = nullptr);
    ~ImuDevice() override;

    QString identifier() const {
        return m_identifier;
    }

    Q_INVOKABLE void setDependencies(Transport* transport, Imu3DMGX3ProtocolParser* parser);

    Q_INVOKABLE bool initialize() override;
    void shutdown() override;
    DeviceType type() const override {
        return DeviceType::Imu;
    }

    Q_INVOKABLE void setPollInterval(int intervalMs);

signals:
    void imuDataChanged(const ImuData& data);

private slots:
    void pollTimerTimeout();
    void processFrame(const QByteArray& frame);
    void processMessage(const Message& message);
    void onCommunicationWatchdogTimeout();
    void onGyroBiasTimeout();

private:
    void sendReadRequest();
    void captureGyroBias();
    void startPolling();
    void resetCommunicationWatchdog();
    void setConnectionState(bool connected);

    QString m_identifier;
    Transport* m_transport = nullptr;
    Imu3DMGX3ProtocolParser* m_parser = nullptr;

    QTimer* m_pollTimer;
    QTimer* m_communicationWatchdog = nullptr;
    QTimer* m_gyroBiasTimer = nullptr;

    bool m_waitingForGyroBias = false;
    int m_pollIntervalMs = 10;

    static constexpr int COMMUNICATION_TIMEOUT_MS = 3000;
    static constexpr int GYRO_BIAS_TIMEOUT_MS = 15000;  // 15 seconds (10s capture + margin)
};


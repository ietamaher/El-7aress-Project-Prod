#pragma once

/**
 * @file nightcameracontroldevice.h
 * @brief Refactored Night Camera device following MIL-STD architecture
 *
 * @author refactored_to_milstd
 * @date 2025-10-28
 * @version 2.0
 */

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
class NightCameraProtocolParser;
class Message;

class NightCameraControlDevice : public TemplatedDevice<NightCameraData> {
    Q_OBJECT

public:
    explicit NightCameraControlDevice(const QString& identifier, QObject* parent = nullptr);
    ~NightCameraControlDevice() override;

    QString identifier() const {
        return m_identifier;
    }

    Q_INVOKABLE void setDependencies(Transport* transport, NightCameraProtocolParser* parser);
    Q_INVOKABLE bool initialize() override;
    void shutdown() override;
    DeviceType type() const override {
        return DeviceType::NightCamera;
    }

    // Camera controls
    Q_INVOKABLE void performFFC();
    Q_INVOKABLE void setDigitalZoom(quint8 zoomLevel);
    Q_INVOKABLE void setVideoModeLUT(quint16 mode);
    Q_INVOKABLE void getCameraStatus();
    Q_INVOKABLE void readFpaTemperature();
    Q_INVOKABLE void setPanTilt(qint16 tilt, qint16 pan);
    Q_INVOKABLE void getVideoMode();  // Query current zoom mode
    Q_INVOKABLE void getVideoLUT();   // Query current LUT

signals:
    void nightCameraDataChanged(const NightCameraData& data);

private slots:
    void processFrame(const QByteArray& frame);
    void processMessage(const Message& message);
    void checkCameraStatus();
    void onCommunicationWatchdogTimeout();

private:
    void sendCommand(quint8 function, const QByteArray& data);
    void resetCommunicationWatchdog();
    void setConnectionState(bool connected);

    QString m_identifier;
    Transport* m_transport = nullptr;
    NightCameraProtocolParser* m_parser = nullptr;
    QTimer* m_statusCheckTimer = nullptr;
    QTimer* m_communicationWatchdog = nullptr;

    static constexpr int COMMUNICATION_TIMEOUT_MS = 10000;  // 3 seconds without data = disconnected
};


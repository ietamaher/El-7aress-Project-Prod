/**
 * @file daycameracontroldevice.h
 * @brief Refactored Day Camera device following MIL-STD architecture
 *
 * @author refactored_to_milstd
 * @date 2025-10-28
 * @version 2.0
 */

#ifndef DAYCAMERACONTROLDEVICE_H
#define DAYCAMERACONTROLDEVICE_H

#include "../devices/TemplatedDevice.h"
#include "../data/DataTypes.h"

class Transport;
class DayCameraProtocolParser;
class Message;

class DayCameraControlDevice : public TemplatedDevice<DayCameraData> {
    Q_OBJECT

public:
    explicit DayCameraControlDevice(const QString& identifier, QObject* parent = nullptr);
    ~DayCameraControlDevice() override;

    QString identifier() const {
        return m_identifier;
    }

    Q_INVOKABLE void setDependencies(Transport* transport, DayCameraProtocolParser* parser);
    Q_INVOKABLE bool initialize() override;
    void shutdown() override;
    DeviceType type() const override {
        return DeviceType::DayCamera;
    }

    // Zoom controls
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomStop();
    Q_INVOKABLE void setZoomPosition(quint16 position);

    // Focus controls
    Q_INVOKABLE void focusNear();
    Q_INVOKABLE void focusFar();
    Q_INVOKABLE void focusStop();
    Q_INVOKABLE void setFocusAuto(bool enabled);
    Q_INVOKABLE void setFocusPosition(quint16 position);

    Q_INVOKABLE void getCameraStatus();

signals:
    void dayCameraDataChanged(const DayCameraData& data);

private slots:
    void processFrame(const QByteArray& frame);
    void processMessage(const Message& message);
    void checkCameraStatus();
    void onCommunicationWatchdogTimeout();

private:
    void sendCommand(quint8 cmd1, quint8 cmd2, quint8 data1 = 0, quint8 data2 = 0);
    void resetCommunicationWatchdog();
    void setConnectionState(bool connected);
    void pauseStatusCheckDuringZoom();
    void resumeStatusCheckAfterZoom();

    QString m_identifier;
    Transport* m_transport = nullptr;
    DayCameraProtocolParser* m_parser = nullptr;
    QTimer* m_statusCheckTimer = nullptr;
    QTimer* m_communicationWatchdog = nullptr;
    bool m_zoomActive = false;  // Track if zoom operation is in progress

    static constexpr int COMMUNICATION_TIMEOUT_MS =
        15000;  // 15 seconds without data = disconnected
    static constexpr int STATUS_CHECK_INTERVAL_MS = 10000;  // Status check interval
};

#endif  // DAYCAMERACONTROLDEVICE_H

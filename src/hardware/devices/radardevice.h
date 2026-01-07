/**
 * @file radardevice.h
 * @brief Refactored Radar device following MIL-STD architecture
 *
 * This class represents ONLY the device logic - no transport or protocol handling.
 * Transport and protocol parsing are injected as dependencies.
 *
 * @section Architecture
 * - Device: Pure business logic (this class)
 * - Transport: SerialPortTransport (injected)
 * - Parser: RadarProtocolParser (injected)
 * - Data: RadarData (in DataTypes.h)
 *
 * @section Benefits
 * - Clean separation of concerns
 * - Thread-safe data access (automatic via TemplatedDevice)
 * - Easy unit testing (mock transport/parser)
 * - Protocol changes isolated to parser
 *
 * @author refactored_to_milstd
 * @date 2025-10-28
 * @version 2.0
 */

#ifndef RADARDEVICE_H
#define RADARDEVICE_H

#include "../devices/TemplatedDevice.h"
#include "../data/DataTypes.h"
#include <QVector>
#include <QTimer>

class Transport;
class RadarProtocolParser;
class Message;

/**
 * @brief NMEA 0183-based radar device
 *
 * Manages a radar via NMEA 0183 protocol (RATTM sentences). This class contains
 * ONLY device-specific logic - all transport and protocol handling
 * is delegated to injected dependencies.
 *
 * Tracks multiple radar targets and emits updates when new plots are received.
 */
class RadarDevice : public TemplatedDevice<RadarData> {
    Q_OBJECT

public:
    explicit RadarDevice(const QString& identifier, QObject* parent = nullptr);
    ~RadarDevice() override;

    // Device identification
    QString identifier() const {
        return m_identifier;
    }

    // Dependency injection (called before initialize)
    Q_INVOKABLE void setDependencies(Transport* transport, RadarProtocolParser* parser);

    // IDevice interface (device lifecycle)
    Q_INVOKABLE bool initialize() override;
    void shutdown() override;
    DeviceType type() const override {
        return DeviceType::Radar;
    }

    // Public API - Target management
    Q_INVOKABLE QVector<RadarData> trackedTargets() const;
    Q_INVOKABLE void clearTrackedTargets();

signals:
    void radarPlotsUpdated(const QVector<RadarData>& plots);
    void newPlotReceived(const RadarData& plot);

private slots:
    void processFrame(const QByteArray& frame);
    void processMessage(const Message& message);
    void onCommunicationWatchdogTimeout();

private:
    void updateTrackedTarget(const RadarData& newPlot);
    void resetCommunicationWatchdog();
    void setConnectionState(bool connected);

    QString m_identifier;
    Transport* m_transport = nullptr;
    RadarProtocolParser* m_parser = nullptr;

    QTimer* m_communicationWatchdog = nullptr;
    QVector<RadarData> m_trackedTargets;  // Multiple tracked targets

    static constexpr int COMMUNICATION_TIMEOUT_MS =
        10000;  // 10 seconds without data = disconnected
};

#endif  // RADARDEVICE_H

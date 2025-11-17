/**
 * @file plc21device.h
 * @brief Refactored PLC21 device following MIL-STD architecture
 *
 * This class represents ONLY the device logic - no transport or protocol handling.
 * Transport and protocol parsing are injected as dependencies.
 *
 * @section Architecture
 * - Device: Pure business logic (this class)
 * - Transport: ModbusTransport (injected)
 * - Parser: Plc21ProtocolParser (injected)
 * - Data: Plc21PanelData (in DataTypes.h)
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

#ifndef PLC21DEVICE_H
#define PLC21DEVICE_H

#include "../devices/TemplatedDevice.h"
#include "../data/DataTypes.h"
#include <QTimer>

class Transport;
class Plc21ProtocolParser;
class QModbusReply;
class Message;

/**
 * @brief Modbus-based PLC21 panel device
 *
 * Manages a PLC21 control panel via Modbus RTU protocol. This class contains
 * ONLY device-specific logic - all transport and protocol handling
 * is delegated to injected dependencies.
 */
class Plc21Device : public TemplatedDevice<Plc21PanelData> {
    Q_OBJECT
public:
    explicit Plc21Device(const QString& identifier, QObject* parent = nullptr);
    ~Plc21Device() override;

    // Device identification
    QString identifier() const { return m_identifier; }

    // Dependency injection (called before initialize)
    Q_INVOKABLE void setDependencies(Transport* transport,
                                      Plc21ProtocolParser* parser);

    // IDevice interface (device lifecycle)
    Q_INVOKABLE bool initialize() override;
    void shutdown() override;
    DeviceType type() const override { return DeviceType::Plc21; }

    // Public API - Write outputs
    Q_INVOKABLE void setDigitalOutputs(const QVector<bool>& outputs);
    Q_INVOKABLE void writeDigitalOutput(int index, bool value);
    Q_INVOKABLE void setGunArmedLed(bool on);
    Q_INVOKABLE void setStationEnabledLed(bool on);
    Q_INVOKABLE void sethatchStateLed(bool on);
    Q_INVOKABLE void setPanelBacklight(bool on);
    // Configuration
    Q_INVOKABLE void setPollInterval(int intervalMs);

signals:
    void panelDataChanged(const Plc21PanelData& data);
    void digitalOutputWritten(bool success);

private slots:
    void pollTimerTimeout();
    void onModbusReplyReady(QModbusReply* reply);
    void processMessage(const Message& message);
    void onCommunicationWatchdogTimeout();

private:
    void sendReadRequest(int startAddress, int count, bool isDiscreteInputs = true);
    void sendWriteRequest(int startAddress, const QVector<bool>& values);
    void mergePartialData(const Plc21PanelData& partialData);
    void resetCommunicationWatchdog();
    void setConnectionState(bool connected);
    void sendNextPendingRequest();
    void startPollCycle();  // Start a new poll cycle

    QString m_identifier;
    Transport* m_transport = nullptr;
    Plc21ProtocolParser* m_parser = nullptr;

    QTimer* m_pollTimer;
    QTimer* m_communicationWatchdog = nullptr;
    QVector<bool> m_digitalOutputs; // Cached output state for writing

    // Request sequencing to prevent concurrent Modbus requests
    bool m_waitingForResponse = false;
    bool m_needsHoldingRegistersRead = false;
    bool m_pollCycleActive = false;  // Track if a poll cycle is in progress

    static constexpr int COMMUNICATION_TIMEOUT_MS = 3000;  // 3 seconds without data = disconnected
};

#endif // PLC21DEVICE_H

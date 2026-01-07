/**
 * @file ServoActuatorDevice.h
 * @brief Refactored serial servo actuator device following MIL-STD architecture
 *
 * This class represents ONLY the device logic - no transport or protocol handling.
 * Transport and protocol parsing are injected as dependencies.
 *
 * @section Architecture
 * - Device: Pure business logic (this class)
 * - Transport: SerialPortTransport (injected)
 * - Parser: ServoActuatorProtocolParser (injected)
 * - Data: ServoActuatorData (in DataTypes.h)
 *
 * @section Benefits
 * - 62% code reduction (400 lines → 150 lines)
 * - Thread-safe data access (automatic via TemplatedDevice)
 * - Easy unit testing (mock transport/parser)
 * - Protocol changes isolated to parser
 * - Command queue managed internally
 *
 * @author refactored_to_milstd
 * @date 2025-10-28
 * @version 2.0
 */

#ifndef SERVOACTUATORDEVICE_H
#define SERVOACTUATORDEVICE_H

#include "../devices/TemplatedDevice.h"
#include "../data/DataTypes.h"
#include <QTimer>
#include <QQueue>

class Transport;
class ServoActuatorProtocolParser;
class Message;

/**
 * @brief Serial ASCII-based servo actuator device
 *
 * Manages a servo actuator via serial ASCII protocol. This class contains
 * ONLY device-specific logic - all transport and protocol handling
 * is delegated to injected dependencies.
 */
class ServoActuatorDevice : public TemplatedDevice<ServoActuatorData> {
    Q_OBJECT
public:
    explicit ServoActuatorDevice(const QString& identifier, QObject* parent = nullptr);
    ~ServoActuatorDevice() override;

    // Device identification
    QString identifier() const { return m_identifier; }

    // Dependency injection (called before initialize)
    Q_INVOKABLE void setDependencies(Transport* transport,
                                      ServoActuatorProtocolParser* parser);

    // IDevice interface (device lifecycle)
    Q_INVOKABLE bool initialize() override;
    void shutdown() override;
    DeviceType type() const override { return DeviceType::ServoActuator; }

    // Public API - Motion Control
    Q_INVOKABLE void moveToPosition(double position_mm);
    Q_INVOKABLE void setMaxSpeed(double speed_mm_s);
    Q_INVOKABLE void setAcceleration(double accel_mm_s2);
    Q_INVOKABLE void setMaxTorque(double percent);
    Q_INVOKABLE void stopMove();
    Q_INVOKABLE void holdCurrentPosition();

    // Public API - Status & Diagnostics
    Q_INVOKABLE void checkAllStatus();
    Q_INVOKABLE void checkStatusRegister();
    Q_INVOKABLE void checkPosition();
    Q_INVOKABLE void checkVelocity();
    Q_INVOKABLE void checkTorque();
    Q_INVOKABLE void checkTemperature();
    Q_INVOKABLE void checkBusVoltage();

    // Public API - System & Configuration
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void clearFaults();
    Q_INVOKABLE void reboot();

signals:
    void actuatorDataChanged(const ServoActuatorData& data);
    void commandError(const QString& errorDetails);
    void criticalFaultOccurred(const QStringList& faults);
    void commandAcknowledged(const QString& command);

private slots:
    void onFrameReceived(const QByteArray& frame);
    void processMessage(const Message& message);
    void handleCommandTimeout();
    void processNextCommand();
    void checkActuatorStatus();
    void onTransportDisconnected();
    void onCommunicationWatchdogTimeout();

private:
    void sendCommand(const QString& command);
    void mergePartialData(const ServoActuatorData& partialData);
    void resetCommunicationWatchdog();
    void setConnectionState(bool connected);

    QString m_identifier;
    Transport* m_transport = nullptr;
    ServoActuatorProtocolParser* m_parser = nullptr;

    QTimer* m_commandTimeoutTimer;
    QTimer* m_statusCheckTimer;
    QTimer* m_communicationWatchdog;
    QString m_pendingCommand;
    QQueue<QString> m_commandQueue;

    static constexpr int COMMAND_TIMEOUT_MS = 1000;
    static constexpr int INTER_COMMAND_DELAY_MS = 10;
    static constexpr int STATUS_CHECK_INTERVAL_MS = 50;  // Check status every 50 mseconds
    static constexpr int COMMUNICATION_TIMEOUT_MS = 10000;  // 3 seconds without data = disconnected
};

#endif // SERVOACTUATORDEVICE_H

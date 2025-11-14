#ifndef IDEVICE_H
#define IDEVICE_H

#include <QObject>

class IDevice : public QObject {
    Q_OBJECT
public:
    enum class DeviceState { Offline, Initializing, Online, Error };
    Q_ENUM(DeviceState)

    enum class DeviceType { Unknown, Radar, ServoDriver, ServoActuator, LRF, Camera,  Plc21, Plc42, Imu, DayCamera, NightCamera, Joystick };
    Q_ENUM(DeviceType)

    // Provide the function body (the definition) directly in the header.
    explicit IDevice(QObject* parent = nullptr) : QObject(parent), m_state(DeviceState::Offline) {}

    virtual ~IDevice() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual DeviceType type() const = 0;

    // Provide the definition for state()
    DeviceState state() const { return m_state; }

signals:
    void stateChanged(IDevice::DeviceState newState);
    void deviceError(const QString& message);

protected:
    // Provide the definition for setState()
    void setState(DeviceState newState) {
        if (m_state != newState) {
            m_state = newState;
            emit stateChanged(m_state);
        }
    }

    DeviceState m_state;
};

#endif // IDEVICE_H

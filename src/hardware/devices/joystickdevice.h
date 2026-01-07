#ifndef JOYSTICKDEVICE_H
#define JOYSTICKDEVICE_H

#include <QTimer>
#include <SDL2/SDL.h>
#include "TemplatedDevice.h"
#include "../data/DataTypes.h"

class JoystickProtocolParser;

/**
 * @brief SDL2-based joystick input device
 *
 * This device handles joystick input using the SDL2 library.
 * It specifically targets the Thrustmaster HOTAS Warthog joystick
 * by GUID, but can be adapted for other joysticks.
 *
 * Architecture:
 * - Inherits from TemplatedDevice<JoystickData> for thread-safe data access
 * - Uses SDL2 for joystick communication (SDL is the "transport" layer)
 * - JoystickProtocolParser interprets SDL events into JoystickData
 * - Polls at ~60Hz for responsive input
 *
 * Usage:
 * 1. Create device instance
 * 2. Set parser via setParser()
 * 3. Call initialize() to open joystick
 * 4. Access data via data() method
 * 5. Call shutdown() when done
 */
class JoystickDevice : public TemplatedDevice<JoystickData> {
    Q_OBJECT
public:
    explicit JoystickDevice(QObject* parent = nullptr);
    ~JoystickDevice() override;

    // IDevice interface implementation
    bool initialize() override;
    void shutdown() override;
    DeviceType type() const override { return DeviceType::Joystick; }

    /**
     * @brief Set the protocol parser dependency
     * @param parser The parser to use for interpreting SDL events
     *
     * Must be called before initialize()
     */
    void setParser(JoystickProtocolParser* parser);

    /**
     * @brief Set the target joystick GUID
     * @param guid The SDL joystick GUID string (32 hex characters)
     *
     * Default is Thrustmaster HOTAS Warthog: "030000004f0400000204000011010000"
     * Must be called before initialize()
     */
    void setTargetGUID(const QString& guid);

    /**
     * @brief Set the polling interval in milliseconds
     * @param intervalMs Polling interval (default: 16ms for ~60Hz)
     */
    void setPollInterval(int intervalMs);

    /**
     * @brief Print all connected joystick GUIDs to debug log
     *
     * Useful for identifying the GUID of a connected joystick
     */
    static void printJoystickGUIDs();

signals:
    /**
     * @brief Emitted when joystick data changes (including connection status)
     * @param newData Shared pointer to the new joystick data
     */
    void dataChanged(std::shared_ptr<const JoystickData> newData);

    /**
     * @brief Emitted when an axis moves
     * @param axis Axis index (0 = X, 1 = Y, etc.)
     * @param value Normalized value (-1.0 to 1.0)
     */
    void axisMoved(int axis, float value);

    /**
     * @brief Emitted when a button is pressed or released
     * @param button Button index (0-15)
     * @param pressed True if pressed, false if released
     */
    void buttonPressed(int button, bool pressed);

    /**
     * @brief Emitted when a hat switch moves
     * @param hat Hat index
     * @param value Hat direction value (0=center, 1=up, 2=right, 4=down, 8=left)
     */
    void hatMoved(int hat, int value);

private slots:
    void pollJoystick();

private:
    bool initializeSDL();
    bool openJoystick();
    void emitEventSignals(const SDL_Event& event);

    SDL_Joystick* m_joystick;
    QTimer* m_pollTimer;
    JoystickProtocolParser* m_parser;
    QString m_targetGUID;
    int m_pollInterval;
    bool m_sdlInitialized;
    bool m_lastConnectionState;  // Track connection state to detect changes
};

#endif // JOYSTICKDEVICE_H

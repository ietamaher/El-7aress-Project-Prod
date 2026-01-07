#pragma once

#include <SDL2/SDL.h>
#include "../interfaces/ProtocolParser.h"
#include "../data/DataTypes.h"

/**
 * @brief Protocol parser for SDL2 joystick events
 *
 * This parser interprets SDL joystick events and maintains a JoystickData
 * structure representing the current state of all joystick inputs.
 *
 * Note: SDL2 already handles the low-level USB HID protocol, so this
 * parser's job is to normalize and aggregate SDL events into our data model.
 */
class JoystickProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit JoystickProtocolParser(QObject* parent = nullptr);
    ~JoystickProtocolParser() override = default;

    /**
     * @brief Not used for joystick - SDL events are processed directly
     * @return Empty vector (joystick uses SDL_Event, not raw bytes)
     */
    std::vector<MessagePtr> parse(const QByteArray& /*rawData*/) override {
        return {};  // SDL2 joystick doesn't use byte-level parsing
    }

    /**
     * @brief Process an SDL event and update joystick state
     * @param event The SDL event to process
     * @return Message containing updated joystick data, or nullptr if not a joystick event
     */
    MessagePtr processEvent(const SDL_Event& event);

    /**
     * @brief Get the current joystick state
     * @return Current aggregated joystick data
     */
    const JoystickData& currentState() const {
        return m_currentState;
    }

    /**
     * @brief Reset all joystick state to defaults
     */
    void reset();

private:
    /**
     * @brief Normalize SDL axis value (-32768 to 32767) to float (-1.0 to 1.0)
     * @param value Raw SDL axis value
     * @return Normalized float value
     */
    static float normalizeAxisValue(int16_t value);

    JoystickData m_currentState;
};

#pragma once

#include "../interfaces/Message.h"
#include "../data/DataTypes.h"

/**
 * @brief Message containing joystick input state data
 *
 * This message is generated when the joystick state changes,
 * including axis movements, button presses, and hat switch movements.
 */
class JoystickDataMessage : public Message {
public:
    explicit JoystickDataMessage(const JoystickData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::JoystickDataType;
    }

    const JoystickData& data() const {
        return m_data;
    }

private:
    JoystickData m_data;
};

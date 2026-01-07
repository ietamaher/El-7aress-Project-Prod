#include "JoystickProtocolParser.h"
#include "../messages/JoystickMessage.h"
#include <QDebug>
#include <cmath>

JoystickProtocolParser::JoystickProtocolParser(QObject* parent)
    : ProtocolParser(parent), m_currentState() {
    reset();
}

MessagePtr JoystickProtocolParser::processEvent(const SDL_Event& event) {
    bool stateChanged = false;

    switch (event.type) {
    case SDL_JOYAXISMOTION: {
        // Handle axis motion events
        int axis = event.jaxis.axis;
        float normalizedValue = normalizeAxisValue(event.jaxis.value);

        // Update the appropriate axis (we track X and Y)
        if (axis == 0) {
            // X axis
            if (!qFuzzyCompare(m_currentState.axisX, normalizedValue)) {
                m_currentState.axisX = normalizedValue;
                stateChanged = true;
            }
        } else if (axis == 1) {
            // Y axis
            if (!qFuzzyCompare(m_currentState.axisY, normalizedValue)) {
                m_currentState.axisY = normalizedValue;
                stateChanged = true;
            }
        }
        // Additional axes could be added here if needed

        break;
    }

    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP: {
        // Handle button press/release events
        int button = event.jbutton.button;
        bool pressed = (event.type == SDL_JOYBUTTONDOWN);

        if (button >= 0 && button < JoystickData::MAX_BUTTONS) {
            if (m_currentState.buttons[button] != pressed) {
                m_currentState.buttons[button] = pressed;
                stateChanged = true;
            }
        } else {
            qWarning() << "Button index out of range:" << button;
        }

        break;
    }

    case SDL_JOYHATMOTION: {
        // Handle hat switch motion events
        // SDL hat values: centered=0, up=1, right=2, down=4, left=8
        // Diagonal values are combinations (e.g., up-right = 3)
        int hatValue = event.jhat.value;

        if (m_currentState.hatState != hatValue) {
            m_currentState.hatState = hatValue;
            stateChanged = true;
        }

        break;
    }

    default:
        // Not a joystick event we care about
        return nullptr;
    }

    // If state changed, return a message with the updated data
    if (stateChanged) {
        return std::make_unique<JoystickDataMessage>(m_currentState);
    }

    return nullptr;
}

void JoystickProtocolParser::reset() {
    m_currentState.isConnected = false;
    m_currentState.axisX = 0.0f;
    m_currentState.axisY = 0.0f;
    m_currentState.hatState = 0;

    for (int i = 0; i < JoystickData::MAX_BUTTONS; ++i) {
        m_currentState.buttons[i] = false;
    }
}

float JoystickProtocolParser::normalizeAxisValue(int16_t value) {
    // Apply deadzone to filter out joystick drift
    const int16_t DEADZONE = 3000;  // ~9% deadzone

    if (std::abs(value) < DEADZONE) {
        return 0.0f;
    }

    // Normalize to -1.0 to 1.0 range
    // SDL axis range: -32768 to 32767
    if (value < 0) {
        return static_cast<float>(value + DEADZONE) / (32768.0f - DEADZONE);
    } else {
        return static_cast<float>(value - DEADZONE) / (32767.0f - DEADZONE);
    }
}

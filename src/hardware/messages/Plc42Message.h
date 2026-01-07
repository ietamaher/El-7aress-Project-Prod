#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"

//================================================================================
// PLC42 MESSAGES (Modbus-based)
//================================================================================

/**
 * @brief Message carrying PLC42 data
 */
class Plc42DataMessage : public Message {
public:
    explicit Plc42DataMessage(const Plc42Data& data) : m_data(data) {}

    Type typeId() const override {
        return Type::Plc42DataType;
    }
    const Plc42Data& data() const {
        return m_data;
    }

private:
    Plc42Data m_data;
};

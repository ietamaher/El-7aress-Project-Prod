#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"

//================================================================================
// PLC21 MESSAGES (Modbus-based)
//================================================================================

/**
 * @brief Message carrying PLC21 panel data
 */
class Plc21DataMessage : public Message {
public:
    explicit Plc21DataMessage(const Plc21PanelData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::Plc21DataType;
    }
    const Plc21PanelData& data() const {
        return m_data;
    }

private:
    Plc21PanelData m_data;
};

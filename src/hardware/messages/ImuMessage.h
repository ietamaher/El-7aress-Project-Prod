#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"

//================================================================================
// IMU MESSAGES (Modbus-based)
//================================================================================

/**
 * @brief Message carrying IMU/Inclinometer data
 */
class ImuDataMessage : public Message {
public:
    explicit ImuDataMessage(const ImuData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::ImuDataType;
    }
    const ImuData& data() const {
        return m_data;
    }

private:
    ImuData m_data;
};

#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"

//================================================================================
// NIGHT CAMERA MESSAGES (TAU2 based)
//================================================================================

/**
 * @brief Message carrying night camera data
 */
class NightCameraDataMessage : public Message {
public:
    explicit NightCameraDataMessage(const NightCameraData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::NightCameraDataType;
    }
    const NightCameraData& data() const {
        return m_data;
    }

private:
    NightCameraData m_data;
};

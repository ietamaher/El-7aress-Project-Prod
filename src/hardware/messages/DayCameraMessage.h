#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"

//================================================================================
// DAY CAMERA MESSAGES (Pelco-D based)
//================================================================================

/**
 * @brief Message carrying day camera data
 */
class DayCameraDataMessage : public Message {
public:
    explicit DayCameraDataMessage(const DayCameraData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::DayCameraDataType;
    }
    const DayCameraData& data() const {
        return m_data;
    }

private:
    DayCameraData m_data;
};

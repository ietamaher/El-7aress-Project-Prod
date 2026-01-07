#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"
#include <QVector>

//================================================================================
// RADAR MESSAGES (NMEA-based)
//================================================================================

/**
 * @brief Message carrying radar plot data from NMEA RATTM sentence
 */
class RadarPlotMessage : public Message {
public:
    explicit RadarPlotMessage(const RadarData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::RadarPlotType;
    }
    const RadarData& data() const {
        return m_data;
    }

private:
    RadarData m_data;
};

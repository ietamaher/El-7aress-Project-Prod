#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"
#include <QString>

//================================================================================
// SERVO DRIVER MESSAGES (Modbus-based)
//================================================================================

/**
 * @brief Message carrying servo driver telemetry data
 */
class ServoDriverDataMessage : public Message {
public:
    explicit ServoDriverDataMessage(const ServoDriverData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::ServoDriverDataType;
    }
    const ServoDriverData& data() const {
        return m_data;
    }

private:
    ServoDriverData m_data;
};

/**
 * @brief Message carrying a specific alarm code and description
 */
class ServoDriverAlarmMessage : public Message {
public:
    explicit ServoDriverAlarmMessage(uint16_t code, const QString& desc)
        : m_alarmCode(code), m_description(desc) {}

    Type typeId() const override {
        return Type::ServoDriverAlarmType;
    }
    uint16_t alarmCode() const {
        return m_alarmCode;
    }
    const QString& description() const {
        return m_description;
    }

private:
    uint16_t m_alarmCode;
    QString m_description;
};

/**
 * @brief Message carrying alarm history data
 */
class ServoDriverAlarmHistoryMessage : public Message {
public:
    explicit ServoDriverAlarmHistoryMessage(const QList<uint16_t>& history)
        : m_alarmHistory(history) {}

    Type typeId() const override {
        return Type::ServoDriverAlarmHistoryType;
    }
    const QList<uint16_t>& alarmHistory() const {
        return m_alarmHistory;
    }

private:
    QList<uint16_t> m_alarmHistory;
};

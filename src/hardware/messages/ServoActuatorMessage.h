#pragma once
#include "../interfaces/Message.h"
#include "../data/DataTypes.h"
#include <QString>

//================================================================================
// SERVO ACTUATOR MESSAGES (Serial-based)
//================================================================================

/**
 * @brief Message carrying servo actuator telemetry data
 */
class ServoActuatorDataMessage : public Message {
public:
    explicit ServoActuatorDataMessage(const ServoActuatorData& data) : m_data(data) {}

    Type typeId() const override {
        return Type::ServoActuatorDataType;
    }
    const ServoActuatorData& data() const {
        return m_data;
    }

private:
    ServoActuatorData m_data;
};

/**
 * @brief Message indicating command acknowledgment (ACK)
 */
class ServoActuatorAckMessage : public Message {
public:
    explicit ServoActuatorAckMessage(const QString& cmd, const QString& response = "")
        : m_command(cmd), m_response(response) {}

    Type typeId() const override {
        return Type::ServoActuatorAckType;
    }
    const QString& command() const {
        return m_command;
    }
    const QString& response() const {
        return m_response;
    }

private:
    QString m_command;
    QString m_response;
};

/**
 * @brief Message indicating command rejection (NACK)
 */
class ServoActuatorNackMessage : public Message {
public:
    explicit ServoActuatorNackMessage(const QString& cmd, const QString& error)
        : m_command(cmd), m_errorDetails(error) {}

    Type typeId() const override {
        return Type::ServoActuatorNackType;
    }
    const QString& command() const {
        return m_command;
    }
    const QString& errorDetails() const {
        return m_errorDetails;
    }

private:
    QString m_command;
    QString m_errorDetails;
};

/**
 * @brief Message indicating critical fault condition
 */
class ServoActuatorCriticalFaultMessage : public Message {
public:
    explicit ServoActuatorCriticalFaultMessage(const QStringList& faults)
        : m_criticalFaults(faults) {}

    Type typeId() const override {
        return Type::ServoActuatorCriticalFaultType;
    }
    const QStringList& criticalFaults() const {
        return m_criticalFaults;
    }

private:
    QStringList m_criticalFaults;
};

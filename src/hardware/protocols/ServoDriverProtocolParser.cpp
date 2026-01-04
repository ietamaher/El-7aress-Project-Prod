#include "ServoDriverProtocolParser.h"
#include "../messages/ServoDriverMessage.h"
#include "../data/DataTypes.h"
#include <QModbusDataUnit>
#include <QDebug>

ServoDriverProtocolParser::ServoDriverProtocolParser(QObject* parent)
    : ProtocolParser(parent) {
    // Initialize m_data with defaults (connection will be set when data arrives)
    m_data.isConnected = false;
    initializeAlarmMap();
}

std::vector<MessagePtr> ServoDriverProtocolParser::parse(QModbusReply* reply) {
    std::vector<MessagePtr> messages;
    
    if (!reply || reply->error() != QModbusDevice::NoError) {
        return messages;
    }

    const QModbusDataUnit unit = reply->result();

    // Route the reply to the correct parser based on the start address
    switch (unit.startAddress()) {
    case ServoDriverRegisters::POSITION_START_ADDR:
        if (auto msg = parsePositionReply(unit)) {
            messages.push_back(std::move(msg));
        }
        break;
        
    case ServoDriverRegisters::TEMPERATURE_START_ADDR:
        if (auto msg = parseTemperatureReply(unit)) {
            messages.push_back(std::move(msg));
        }
        break;
        
    case ServoDriverRegisters::ALARM_STATUS_ADDR:
        if (auto msg = parseAlarmReply(unit)) {
            messages.push_back(std::move(msg));
        }
        break;
        
    case ServoDriverRegisters::ALARM_HISTORY_ADDR:
        if (auto msg = parseAlarmHistoryReply(unit)) {
            messages.push_back(std::move(msg));
        }
        break;
        
    default:
        qWarning() << "ServoDriverProtocolParser: Unknown register address" 
                   << unit.startAddress();
        break;
    }

    return messages;
}

MessagePtr ServoDriverProtocolParser::parsePositionReply(const QModbusDataUnit& unit) {
    if (unit.valueCount() < ServoDriverRegisters::POSITION_REG_COUNT) {
        qWarning() << "ServoDriverProtocolParser: Insufficient position data";
        return nullptr;
    }

    // ⭐ Update ONLY position field in the accumulated m_data
    m_data.isConnected = true;

    // Combine two 16-bit registers into 32-bit position value
    int32_t positionRaw = (static_cast<int32_t>(unit.value(0)) << 16) | unit.value(1);
    m_data.position = static_cast<float>(positionRaw);

        int32_t speedRaw = (static_cast<int32_t>(unit.value(2)) << 16) | unit.value(3);
    m_data.rpm = static_cast<float>(speedRaw);

        // Torque monitor (registers 10-11)
    if (unit.valueCount() >= 12) {
        int32_t torqueRaw = (static_cast<int32_t>(unit.value(10)) << 16) | unit.value(11);
        m_data.torque = static_cast<float>(torqueRaw);
    } else {
        m_data.torque = 0.0f; // No torque data available
    }

    // Return the accumulated data (temperature fields retain previous values)
    return std::make_unique<ServoDriverDataMessage>(m_data);
}

MessagePtr ServoDriverProtocolParser::parseTemperatureReply(const QModbusDataUnit& unit) {
    if (unit.valueCount() < ServoDriverRegisters::TEMPERATURE_REG_COUNT) {
        qWarning() << "ServoDriverProtocolParser: Insufficient temperature data";
        return nullptr;
    }

    // ⭐ Update ONLY temperature fields in the accumulated m_data
    m_data.isConnected = true;

    // Driver temperature (registers 0-1)
    if (unit.valueCount() < 4) {
        qWarning() << "ServoDriverProtocolParser: Insufficient  registers";
        return nullptr;
    }
    
    int32_t driverTempRaw = (static_cast<int32_t>(unit.value(0)) << 16) | unit.value(1);
    m_data.driverTemp = static_cast<float>(driverTempRaw) * 0.1f;

    // Motor temperature (registers 2-3)
    int32_t motorTempRaw = (static_cast<int32_t>(unit.value(2)) << 16) | unit.value(3);
    m_data.motorTemp = static_cast<float>(motorTempRaw) * 0.1f;

    // Return the accumulated data (position field retains previous value)
    return std::make_unique<ServoDriverDataMessage>(m_data);
}

MessagePtr ServoDriverProtocolParser::parseAlarmReply(const QModbusDataUnit& unit) {
    if (unit.valueCount() < 2) {
        qWarning() << "ServoDriverProtocolParser: Insufficient alarm data";
        return nullptr;
    }

    // Combine upper and lower registers for alarm code
    uint16_t alarmCode = (unit.value(0) << 16) | unit.value(1);
    
    if (alarmCode != 0) {
        QString desc = getAlarmDescription(alarmCode);
        return std::make_unique<ServoDriverAlarmMessage>(alarmCode, desc);
    }
    
    return nullptr;
}

MessagePtr ServoDriverProtocolParser::parseAlarmHistoryReply(const QModbusDataUnit& unit) {
    QList<uint16_t> alarmHistory;
    
    // Process alarm history (2 registers per entry)
    for (int i = 0; i < unit.valueCount(); i += 2) {
        if (i + 1 < unit.valueCount()) {
            uint32_t alarmCode = (static_cast<uint32_t>(unit.value(i)) << 16) | unit.value(i + 1);
            if (alarmCode != 0) { // Only add non-zero alarm codes
                alarmHistory.append(static_cast<uint16_t>(alarmCode));
            }
        }
    }
    
    return std::make_unique<ServoDriverAlarmHistoryMessage>(alarmHistory);
}

QString ServoDriverProtocolParser::getAlarmDescription(uint16_t alarmCode) {
    return m_alarmMap.value(alarmCode, 
        QString("Unknown Alarm: 0x%1").arg(alarmCode, 4, 16, QChar('0')));
}

void ServoDriverProtocolParser::initializeAlarmMap() {
    m_alarmMap[0x0001] = "Overcurrent Alarm";
    m_alarmMap[0x0002] = "Overvoltage Alarm";
    m_alarmMap[0x0003] = "Undervoltage Alarm";
    m_alarmMap[0x0004] = "Overheat Alarm";
    m_alarmMap[0x0005] = "Encoder Error";
    m_alarmMap[0x0006] = "Communication Error";
    // Add more alarm codes as needed
}

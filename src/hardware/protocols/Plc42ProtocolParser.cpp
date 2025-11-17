#include "Plc42ProtocolParser.h"
#include "../messages/Plc42Message.h"
#include <QModbusDataUnit>
#include <QDebug>

Plc42ProtocolParser::Plc42ProtocolParser(QObject* parent)
    : ProtocolParser(parent)
{
    // Initialize m_data with defaults (connection will be set when data arrives)
    m_data.isConnected = false;
}

std::vector<MessagePtr> Plc42ProtocolParser::parse(QModbusReply* reply) {
    std::vector<MessagePtr> messages;

    if (!reply || reply->error() != QModbusDevice::NoError) {
        return messages;
    }

    const QModbusDataUnit unit = reply->result();

    // Determine which type of data we received based on register type and address
    if (unit.registerType() == QModbusDataUnit::DiscreteInputs &&
        unit.startAddress() == Plc42Registers::DIGITAL_INPUTS_START_ADDR) {
        messages.push_back(parseDigitalInputsReply(unit));
    }
    else if (unit.registerType() == QModbusDataUnit::HoldingRegisters &&
             unit.startAddress() == Plc42Registers::HOLDING_REGISTERS_START_ADDR) {
        messages.push_back(parseHoldingRegistersReply(unit));
    }

    return messages;
}

MessagePtr Plc42ProtocolParser::parseDigitalInputsReply(const QModbusDataUnit& unit) {
    // ⭐ Update ONLY digital input fields in the accumulated m_data
    m_data.isConnected = true;

    // Map digital inputs based on their indices
    if (unit.valueCount() >= 8) {
        m_data.stationUpperSensor  = (unit.value(0) != 0);
        m_data.stationLowerSensor  = (unit.value(1) != 0);
        m_data.emergencyStopActive = (unit.value(2) != 0);
        m_data.ammunitionLevel     = (unit.value(3) != 0);
        m_data.hatchState       = (unit.value(4) != 0);
        m_data.stationInput2       = (unit.value(5) != 0);
        m_data.stationInput3       = (unit.value(6) != 0);
        m_data.solenoidActive      = (unit.value(7) != 0);
    }

    // Return the accumulated data (holding registers retain previous values)
    return std::make_unique<Plc42DataMessage>(m_data);
}

MessagePtr Plc42ProtocolParser::parseHoldingRegistersReply(const QModbusDataUnit& unit) {
    // ⭐ Update ONLY holding register fields in the accumulated m_data
    m_data.isConnected = true;

    if (unit.valueCount() >= 7) {
        m_data.solenoidMode = unit.value(0);
        m_data.gimbalOpMode = unit.value(1);

        // Combine two 16-bit registers into a 32-bit value for azimuth speed
        uint16_t azLow  = unit.value(2);
        uint16_t azHigh = unit.value(3);
        m_data.azimuthSpeed = (static_cast<uint32_t>(azHigh) << 16) | azLow;

        // Combine two 16-bit registers into a 32-bit value for elevation speed
        uint16_t elLow  = unit.value(4);
        uint16_t elHigh = unit.value(5);
        m_data.elevationSpeed = (static_cast<uint32_t>(elHigh) << 16) | elLow;

        m_data.azimuthDirection = unit.value(6);

        // Additional registers if available
        if (unit.valueCount() >= 10) {
            m_data.elevationDirection = unit.value(7);
            m_data.solenoidState      = unit.value(8);
            m_data.resetAlarm         = unit.value(9);
        }
    }

    // Return the accumulated data (digital inputs retain previous values)
    return std::make_unique<Plc42DataMessage>(m_data);
}

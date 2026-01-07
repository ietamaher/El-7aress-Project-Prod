#include "Plc21ProtocolParser.h"
#include "../messages/Plc21Message.h"
#include <QModbusDataUnit>
#include <QDebug>

Plc21ProtocolParser::Plc21ProtocolParser(QObject* parent) : ProtocolParser(parent) {
    // Initialize m_data with defaults (connection will be set when data arrives)
    m_data.isConnected = false;
}

std::vector<MessagePtr> Plc21ProtocolParser::parse(QModbusReply* reply) {
    std::vector<MessagePtr> messages;

    if (!reply || reply->error() != QModbusDevice::NoError) {
        return messages;
    }

    const QModbusDataUnit unit = reply->result();

    // Determine which type of data we received based on register type and address
    if (unit.registerType() == QModbusDataUnit::DiscreteInputs &&
        unit.startAddress() == Plc21Registers::DIGITAL_INPUTS_START_ADDR) {
        messages.push_back(parseDigitalInputsReply(unit));
    } else if (unit.registerType() == QModbusDataUnit::HoldingRegisters &&
               unit.startAddress() == Plc21Registers::ANALOG_INPUTS_START_ADDR) {
        messages.push_back(parseAnalogInputsReply(unit));
    }

    return messages;
}

MessagePtr Plc21ProtocolParser::parseDigitalInputsReply(const QModbusDataUnit& unit) {
    // ⭐ Update ONLY digital input fields in the accumulated m_data
    m_data.isConnected = true;

    // Map digital inputs based on their indices
    if (unit.valueCount() > 0) {
        m_data.authorizeSw = (unit.value(0) != 0);
    }
    if (unit.valueCount() > 1) {
        m_data.menuValSw = (unit.value(1) != 0);
    }
    if (unit.valueCount() > 2) {
        m_data.menuDownSW = (unit.value(2) != 0);
    }
    if (unit.valueCount() > 3) {
        m_data.menuUpSW = (unit.value(3) != 0);
    }
    if (unit.valueCount() > 4) {
        m_data.switchCameraSW = (unit.value(4) != 0);
    }
    if (unit.valueCount() > 5) {
        m_data.enableStabilizationSW = (unit.value(5) != 0);
    }
    if (unit.valueCount() > 6) {
        m_data.homePositionSW = (unit.value(6) != 0);
    }
    if (unit.valueCount() > 8) {
        m_data.loadAmmunitionSW = (unit.value(8) != 0);
    }
    if (unit.valueCount() > 9) {
        m_data.armGunSW = (unit.value(9) != 0);
    }
    if (unit.valueCount() > 10) {
        m_data.enableStationSW = (unit.value(10) != 0);
    }

    // Return the accumulated data (analog inputs retain previous values)
    return std::make_unique<Plc21DataMessage>(m_data);
}

MessagePtr Plc21ProtocolParser::parseAnalogInputsReply(const QModbusDataUnit& unit) {
    // ⭐ Update ONLY analog input fields in the accumulated m_data
    m_data.isConnected = true;

    // Map analog inputs based on their indices
    if (unit.valueCount() > 0) {
        m_data.fireMode = unit.value(0);
    }
    if (unit.valueCount() > 1) {
        m_data.speedSW = unit.value(1);
    }
    if (unit.valueCount() > 2) {
        m_data.panelTemperature = unit.value(2);
    }

    // Return the accumulated data (digital inputs retain previous values)
    return std::make_unique<Plc21DataMessage>(m_data);
}

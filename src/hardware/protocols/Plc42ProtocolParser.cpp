#include "Plc42ProtocolParser.h"
#include "../messages/Plc42Message.h"
#include <QModbusDataUnit>
#include <QDebug>

Plc42ProtocolParser::Plc42ProtocolParser(QObject* parent)
    : ProtocolParser(parent)
{
    // Initialize m_data with defaults
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
    // Update ONLY digital input fields in the accumulated m_data
    m_data.isConnected = true;

    // Map 8 physical digital inputs to Plc42Data structure
    // MDUINO 42+ Physical Layout:
    // I0_0 (DI0) → stationUpperSensor
    // I0_1 (DI1) → stationLowerSensor
    // I0_2 (DI2) → hatchState
    // I0_3 (DI3) → freeGimbalState (FREE toggle switch - LOCAL CONTROL)
    // I0_4 (DI4) → ammunitionLevel
    // I0_5 (DI5) → Reserved (future E-STOP button)
    // I0_6 (DI6) → azimuthHomeComplete (Az HOME-END from Oriental Motor) ⭐ NEW
    // I0_7 (DI7) → elevationHomeComplete (El HOME-END from Oriental Motor) ⭐ NEW
    
    if (unit.valueCount() >= 8) {
        m_data.stationUpperSensor     = (unit.value(0) != 0);  // I0_0
        m_data.stationLowerSensor     = (unit.value(1) != 0);  // I0_1
        m_data.hatchState             = (unit.value(2) != 0);  // I0_2
        m_data.freeGimbalState        = (unit.value(3) != 0);  // I0_3 (LOCAL toggle)
        m_data.ammunitionLevel        = (unit.value(4) != 0);  // I0_4
        // unit.value(5) - Reserved for future E-STOP button
        m_data.azimuthHomeComplete    = (unit.value(6) != 0);  // I0_6 ⭐ NEW (Az HOME-END)
        m_data.elevationHomeComplete  = (unit.value(5) != 0);  // I0_7 ⭐ NEW (El HOME-END)
        
        // Note: emergencyStopActive is NOT a physical input - it's derived from
        // gimbalOpMode (set to true when gimbalOpMode == 1)
        // This will be set in parseHoldingRegistersReply()
        
        // Note: solenoidActive is NOT a physical input - it's the output state
        // of the solenoid (Q1_0). Can be set based on solenoidState if needed.
        m_data.solenoidActive = (m_data.solenoidState != 0);
    }

    // Return the accumulated data (holding registers retain previous values)
    return std::make_unique<Plc42DataMessage>(m_data);
}

MessagePtr Plc42ProtocolParser::parseHoldingRegistersReply(const QModbusDataUnit& unit) {
    // Update ONLY holding register fields in the accumulated m_data
    m_data.isConnected = true;

    // Map holding registers to Plc42Data structure
    // HR0: solenoidMode
    // HR1: gimbalOpMode
    // HR2-3: azimuthSpeed (32-bit split across 2 registers)
    // HR4-5: elevationSpeed (32-bit split across 2 registers)
    // HR6: azimuthDirection
    // HR7: elevationDirection
    // HR8: solenoidState
    // HR9: resetAlarm
    // HR10: azimuthReset (0=Normal, 1=Set Preset Home)

    if (unit.valueCount() >= 11) {
        m_data.solenoidMode = unit.value(0);        // HR0
        m_data.gimbalOpMode = unit.value(1);        // HR1

        // Combine two 16-bit registers into 32-bit azimuthSpeed
        uint16_t azLow  = unit.value(2);            // HR2
        uint16_t azHigh = unit.value(3);            // HR3
        m_data.azimuthSpeed = (static_cast<uint32_t>(azHigh) << 16) | azLow;

        // Combine two 16-bit registers into 32-bit elevationSpeed
        uint16_t elLow  = unit.value(4);            // HR4
        uint16_t elHigh = unit.value(5);            // HR5
        m_data.elevationSpeed = (static_cast<uint32_t>(elHigh) << 16) | elLow;

        m_data.azimuthDirection = unit.value(6);    // HR6
        m_data.elevationDirection = unit.value(7);  // HR7
        m_data.solenoidState = unit.value(8);       // HR8
        m_data.resetAlarm = unit.value(9);          // HR9
        m_data.azimuthReset = unit.value(10);       // HR10

        // emergencyStopActive is derived from gimbalOpMode
        // When gimbalOpMode == 1 (GIMBAL_STOP), emergency stop is active
        m_data.emergencyStopActive = (m_data.gimbalOpMode == 1);

        // solenoidActive reflects the solenoid output state
        m_data.solenoidActive = (m_data.solenoidState != 0);
    }

    // Return the accumulated data (digital inputs retain previous values)
    return std::make_unique<Plc42DataMessage>(m_data);
}
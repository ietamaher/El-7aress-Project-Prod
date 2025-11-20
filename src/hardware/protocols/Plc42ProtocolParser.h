#pragma once
#include "../interfaces/ProtocolParser.h"
#include "../data/DataTypes.h"
#include <QModbusReply>

//================================================================================
// PLC42 PROTOCOL PARSER (Modbus RTU)
//================================================================================

/**
 * @brief Register addresses for PLC42 Modbus communication
 */
namespace Plc42Registers {
    // Digital Inputs (Discrete Inputs)
    constexpr int DIGITAL_INPUTS_START_ADDR = 0;
    constexpr int DIGITAL_INPUTS_COUNT = 7;
    
    // Holding Registers
    constexpr int HOLDING_REGISTERS_START_ADDR = 0;
    constexpr int HOLDING_REGISTERS_COUNT = 10;
    
    // Individual Holding Register Indices
    constexpr int HR_SOLENOID_MODE = 0;        // 1=Single, 2=Burst, 3=Continuous
    constexpr int HR_FIRE_RATE = 1;            // 0=Reduced, 1=Full
    constexpr int HR_GIMBAL_OP_MODE = 2;       // 0=Manual, 1=Stop, 3=Home, 4=Free
    constexpr int HR_SPARE_3 = 3;
    constexpr int HR_SPARE_4 = 4;
    constexpr int HR_SPARE_5 = 5;
    constexpr int HR_SPARE_6 = 6;
    constexpr int HR_SPARE_7 = 7;
    constexpr int HR_SOLENOID_STATE = 8;       // 0=OFF, 1=ON (Trigger)
    constexpr int HR_RESET_ALARM = 9;          // 0=Normal, 1=Reset
}

/**
 * @brief Parser for Modbus RTU PLC42 protocol
 *
 * Converts QModbusReply objects into typed Message objects.
 * Handles digital inputs (discrete inputs) and holding registers.
 *
 * IMPORTANT: Maintains accumulated state in m_data since PLC42 data comes from
 * multiple separate Modbus read operations (digital inputs + holding registers).
 */
class Plc42ProtocolParser : public ProtocolParser {
    Q_OBJECT
public:
    explicit Plc42ProtocolParser(QObject* parent = nullptr);
    ~Plc42ProtocolParser() override = default;

    // This parser does not use raw byte streaming
    std::vector<MessagePtr> parse(const QByteArray& /*rawData*/) override { return {}; }

    // Primary parsing method for Modbus replies
    std::vector<MessagePtr> parse(QModbusReply* reply) override;

private:
    // Helper methods to create specific messages from a reply
    MessagePtr parseDigitalInputsReply(const QModbusDataUnit& unit);
    MessagePtr parseHoldingRegistersReply(const QModbusDataUnit& unit);

    // ⭐ Accumulated data state (persists between poll cycles)
    Plc42Data m_data;
};

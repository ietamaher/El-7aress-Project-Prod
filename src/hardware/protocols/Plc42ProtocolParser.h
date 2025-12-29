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
    constexpr int DIGITAL_INPUTS_COUNT = 8;

    // Holding Registers
    constexpr int HOLDING_REGISTERS_START_ADDR = 0;
    constexpr int HOLDING_REGISTERS_COUNT = 11;

    // Individual Holding Register Indices (actual layout from Plc42Device.cpp)
    constexpr int HR_SOLENOID_MODE = 0;        // 1=Single, 2=Burst, 3=Continuous
    constexpr int HR_GIMBAL_OP_MODE = 1;       // 0=Manual, 1=Stop, 3=Home, 4=Free
    constexpr int HR_AZ_SPEED_LOW = 2;         // Azimuth speed low 16 bits
    constexpr int HR_AZ_SPEED_HIGH = 3;        // Azimuth speed high 16 bits
    constexpr int HR_EL_SPEED_LOW = 4;         // Elevation speed low 16 bits
    constexpr int HR_EL_SPEED_HIGH = 5;        // Elevation speed high 16 bits
    constexpr int HR_AZ_DIRECTION = 6;         // Azimuth direction
    constexpr int HR_EL_DIRECTION = 7;         // Elevation direction
    constexpr int HR_SOLENOID_STATE = 8;       // 0=OFF, 1=ON (Trigger)
    constexpr int HR_RESET_ALARM = 9;          // 0=Normal, 1=Reset
    constexpr int HR_AZIMUTH_RESET = 10;       // 0=Normal, 1=Set Preset Home Position
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

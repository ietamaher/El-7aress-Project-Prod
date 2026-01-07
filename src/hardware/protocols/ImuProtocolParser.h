#pragma once
#include "../interfaces/ProtocolParser.h"
#include <QModbusReply>

//================================================================================
// IMU PROTOCOL PARSER (Modbus RTU - SST810)
//================================================================================

/**
 * @brief Register addresses for SST810 IMU Modbus communication
 */
namespace ImuRegisters {
constexpr int ALL_DATA_START_ADDR = 0x03E8;  // 1000 decimal
constexpr int ALL_DATA_REG_COUNT = 18;       // 9 float values * 2 registers
}  // namespace ImuRegisters

/**
 * @brief Parser for Modbus RTU SST810 IMU/Inclinometer protocol
 *
 * Converts QModbusReply objects into typed Message objects.
 * Handles parsing of 32-bit big-endian floats from Input Registers.
 */
class ImuProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit ImuProtocolParser(QObject* parent = nullptr);
    ~ImuProtocolParser() override = default;

    // This parser does not use raw byte streaming
    std::vector<MessagePtr> parse(const QByteArray& /*rawData*/) override {
        return {};
    }

    // Primary parsing method for Modbus replies
    std::vector<MessagePtr> parse(QModbusReply* reply) override;

private:
    // Helper methods
    MessagePtr parseAllDataReply(const QModbusDataUnit& unit);
    float parseFloat(const QModbusDataUnit& unit, int index);
};

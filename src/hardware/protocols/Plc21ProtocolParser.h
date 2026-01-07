#pragma once
#include "../interfaces/ProtocolParser.h"
#include "../data/DataTypes.h"
#include <QModbusReply>

//================================================================================
// PLC21 PROTOCOL PARSER (Modbus RTU)
//================================================================================

/**
 * @brief Register addresses for PLC21 Modbus communication
 */
namespace Plc21Registers {
constexpr int DIGITAL_INPUTS_START_ADDR = 0;
constexpr int DIGITAL_INPUTS_COUNT = 13;
constexpr int ANALOG_INPUTS_START_ADDR = 0;
constexpr int ANALOG_INPUTS_COUNT = 6;
constexpr int DIGITAL_OUTPUTS_START_ADDR = 0;
constexpr int DIGITAL_OUTPUTS_COUNT = 8;
}  // namespace Plc21Registers

/**
 * @brief Parser for Modbus RTU PLC21 protocol
 *
 * Converts QModbusReply objects into typed Message objects.
 * Handles digital inputs (discrete inputs) and analog inputs (holding registers).
 *
 * IMPORTANT: Maintains accumulated state in m_data since PLC21 data comes from
 * multiple separate Modbus read operations (digital inputs + analog inputs).
 */
class Plc21ProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit Plc21ProtocolParser(QObject* parent = nullptr);
    ~Plc21ProtocolParser() override = default;

    // This parser does not use raw byte streaming
    std::vector<MessagePtr> parse(const QByteArray& /*rawData*/) override {
        return {};
    }

    // Primary parsing method for Modbus replies
    std::vector<MessagePtr> parse(QModbusReply* reply) override;

private:
    // Helper methods to create specific messages from a reply
    MessagePtr parseDigitalInputsReply(const QModbusDataUnit& unit);
    MessagePtr parseAnalogInputsReply(const QModbusDataUnit& unit);

    // ⭐ Accumulated data state (persists between poll cycles)
    Plc21PanelData m_data;
};

#pragma once
#include "../interfaces/ProtocolParser.h"
#include <QByteArray>

//================================================================================
// RADAR PROTOCOL PARSER (NMEA 0183)
//================================================================================

/**
 * @brief Parser for NMEA 0183 radar protocol (RATTM sentences)
 *
 * Parses NMEA 0183 $RATTM (Radar Automatic Target Tracking Message) sentences
 * into typed Message objects. Handles checksum validation and field extraction.
 */
class RadarProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit RadarProtocolParser(QObject* parent = nullptr);
    ~RadarProtocolParser() override = default;

    // Primary parsing method for raw NMEA data
    std::vector<MessagePtr> parse(const QByteArray& rawData) override;

    // Modbus not used for radar
    std::vector<MessagePtr> parse(QModbusReply* /*reply*/) override {
        return {};
    }

private:
    // Helper methods
    bool validateChecksum(const QByteArray& sentence);
    MessagePtr parseRATTM(const QByteArray& sentence);

    QByteArray m_buffer;  // Buffer for incomplete NMEA sentences
};

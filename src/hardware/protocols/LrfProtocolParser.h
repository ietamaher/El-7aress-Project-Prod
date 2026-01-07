#ifndef LRFPROTOCOLPARSER_H
#define LRFPROTOCOLPARSER_H

#include "hardware/interfaces/ProtocolParser.h"
#include "hardware/data/DataTypes.h"
#include <vector>

/**
 * @brief Protocol parser for Jioptics LRF devices
 *
 * Implements the 9-byte packet protocol used by Jioptics laser range finders.
 * Handles parsing of ranging responses, status information, and command building.
 */
class LrfProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit LrfProtocolParser(QObject* parent = nullptr);
    ~LrfProtocolParser() override = default;

    // ProtocolParser interface
    std::vector<MessagePtr> parse(const QByteArray& rawData) override;

    /**
     * @brief Build command packet for transmission
     * @param commandCode Command code byte
     * @param params Parameter bytes (default empty, padded to 5 bytes)
     * @return Complete 9-byte packet with checksum
     */
    QByteArray buildCommand(quint8 commandCode, const QByteArray& params = QByteArray());

private:
    // Protocol constants
    static const int PACKET_SIZE = 9;
    static const quint8 FRAME_HEADER = 0xEE;
    enum DeviceCode : quint8 { LRF = 0x07 };

    // Protocol logic
    quint8 calculateChecksum(const QByteArray &body) const;
    bool verifyChecksum(const QByteArray &packet) const;
    MessagePtr handleResponse(const QByteArray &response);

    // Read buffer for partial packets
    QByteArray m_readBuffer;
};

#endif // LRFPROTOCOLPARSER_H

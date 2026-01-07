#pragma once
#include "../interfaces/ProtocolParser.h"

//================================================================================
// NIGHT CAMERA PROTOCOL PARSER (TAU2)
//================================================================================

class NightCameraProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit NightCameraProtocolParser(QObject* parent = nullptr);
    ~NightCameraProtocolParser() override = default;

    std::vector<MessagePtr> parse(const QByteArray& rawData) override;
    std::vector<MessagePtr> parse(QModbusReply* /*reply*/) override {
        return {};
    }

    // Command building
    QByteArray buildCommand(quint8 function, const QByteArray& data);

private:
    quint16 calculateCRC(const QByteArray& data, int length);
    bool verifyCRC(const QByteArray& packet);
    MessagePtr parsePacket(const QByteArray& packet);

    QByteArray m_buffer;
};

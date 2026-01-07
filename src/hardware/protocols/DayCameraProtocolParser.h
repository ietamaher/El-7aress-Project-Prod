#pragma once
#include "../interfaces/ProtocolParser.h"

//================================================================================
// DAY CAMERA PROTOCOL PARSER (Pelco-D)
//================================================================================

class DayCameraProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit DayCameraProtocolParser(QObject* parent = nullptr);
    ~DayCameraProtocolParser() override = default;

    std::vector<MessagePtr> parse(const QByteArray& rawData) override;
    std::vector<MessagePtr> parse(QModbusReply* /*reply*/) override {
        return {};
    }

    // Command building
    QByteArray buildCommand(quint8 cmd1, quint8 cmd2, quint8 data1 = 0, quint8 data2 = 0);

    // HFOV calculation
    double computeHFOVfromZoom(quint16 zoomPos) const;

private:
    bool validateChecksum(const QByteArray& frame);
    MessagePtr parseFrame(const QByteArray& frame);

    QByteArray m_buffer;
    static const quint8 CAMERA_ADDRESS = 0x01;
};

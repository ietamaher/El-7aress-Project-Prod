#include "DayCameraProtocolParser.h"
#include "../messages/DayCameraMessage.h"
#include <QDebug>

DayCameraProtocolParser::DayCameraProtocolParser(QObject* parent)
    : ProtocolParser(parent) {}

std::vector<MessagePtr> DayCameraProtocolParser::parse(const QByteArray& rawData) {
    std::vector<MessagePtr> messages;
    m_buffer.append(rawData);

    // Pelco-D frames are 7 bytes
    while (m_buffer.size() >= 7) {
        if (static_cast<quint8>(m_buffer.at(0)) != 0xFF) {
            m_buffer.remove(0, 1);
            continue;
        }

        QByteArray frame = m_buffer.left(7);
        m_buffer.remove(0, 7);

        if (validateChecksum(frame)) {
            auto msg = parseFrame(frame);
            if (msg) messages.push_back(std::move(msg));
        }
    }
    return messages;
}

bool DayCameraProtocolParser::validateChecksum(const QByteArray& frame) {
    quint8 addr = static_cast<quint8>(frame.at(1));
    quint8 resp1 = static_cast<quint8>(frame.at(2));
    quint8 resp2 = static_cast<quint8>(frame.at(3));
    quint8 data1 = static_cast<quint8>(frame.at(4));
    quint8 data2 = static_cast<quint8>(frame.at(5));
    quint8 recvChecksum = static_cast<quint8>(frame.at(6));
    quint8 calcChecksum = (addr + resp1 + resp2 + data1 + data2) & 0xFF;
    return (recvChecksum == calcChecksum);
}

MessagePtr DayCameraProtocolParser::parseFrame(const QByteArray& frame) {
    DayCameraData data;
    data.isConnected = true;

    quint8 resp2 = static_cast<quint8>(frame.at(3));
    quint8 data1 = static_cast<quint8>(frame.at(4));
    quint8 data2 = static_cast<quint8>(frame.at(5));

    if (resp2 == 0xA7) {
        // Zoom position response
        quint16 zoomPos = (data1 << 8) | data2;
        data.zoomPosition = zoomPos;
        data.currentHFOV = computeHFOVfromZoom(zoomPos);
        data.currentVFOV = data.currentHFOV * (3.0 / 4.0);  // 16:9 aspect ratio → VFOV = HFOV × 9/16
    } else if (resp2 == 0x63) {
        // Focus position response
        quint16 focusPos = (data1 << 8) | data2;
        data.focusPosition = focusPos;
    }

    return std::make_unique<DayCameraDataMessage>(data);
}

QByteArray DayCameraProtocolParser::buildCommand(quint8 cmd1, quint8 cmd2, quint8 data1, quint8 data2) {
    QByteArray packet;
    packet.append((char)0xFF);
    packet.append((char)CAMERA_ADDRESS);
    packet.append((char)cmd1);
    packet.append((char)cmd2);
    packet.append((char)data1);
    packet.append((char)data2);
    quint8 checksum = (CAMERA_ADDRESS + cmd1 + cmd2 + data1 + data2) & 0xFF;
    packet.append((char)checksum);
    return packet;
}

double DayCameraProtocolParser::computeHFOVfromZoom(quint16 zoomPos) const {
    const quint16 maxZoom = 0x4000;
    double fraction = qMin((double)zoomPos / maxZoom, 1.0);
    double wideHFOV = 63.7;
    double teleHFOV = 2.3;
    return wideHFOV - (wideHFOV - teleHFOV) * fraction;
}

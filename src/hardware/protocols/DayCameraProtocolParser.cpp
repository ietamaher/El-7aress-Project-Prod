#include "DayCameraProtocolParser.h"
#include "../messages/DayCameraMessage.h"
#include <QDebug>

DayCameraProtocolParser::DayCameraProtocolParser(QObject* parent) : ProtocolParser(parent) {}

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
            if (msg)
                messages.push_back(std::move(msg));
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
        data.currentVFOV =
            2.0 * atan(tan(data.currentHFOV * M_PI / 360.0) * (3.0 / 4.0)) * 180.0 / M_PI;

    } else if (resp2 == 0x63) {
        // Focus position response
        quint16 focusPos = (data1 << 8) | data2;
        data.focusPosition = focusPos;
    }

    return std::make_unique<DayCameraDataMessage>(data);
}

QByteArray DayCameraProtocolParser::buildCommand(quint8 cmd1, quint8 cmd2, quint8 data1,
                                                 quint8 data2) {
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

/*double DayCameraProtocolParser::computeHFOVfromZoom(quint16 zoomPos) const {
    const quint16 maxZoom = 16384;
    double fraction = qMin((double)zoomPos / maxZoom, 1.0);
    double wideHFOV = 45.8;
    double teleHFOV = 1.53;
    return wideHFOV - (wideHFOV - teleHFOV) * fraction;
}*/

double DayCameraProtocolParser::computeHFOVfromZoom(quint16 zoomPos) const {
    static const std::pair<int, double> sonyTable[] = {
        {0x0000, 1.0},  {0x16A1, 2.0},  {0x2063, 3.0},  {0x2628, 4.0},  {0x2A1D, 5.0},
        {0x2D13, 6.0},  {0x2F6D, 7.0},  {0x3161, 8.0},  {0x330D, 9.0},  {0x3486, 10.0},
        {0x3709, 12.0}, {0x3920, 14.0}, {0x3ADD, 16.0}, {0x3C46, 18.0}, {0x3D60, 20.0},
        {0x3E90, 23.0}, {0x3EDC, 24.0}, {0x3F57, 26.0}, {0x3FB6, 28.0}, {0x4000, 30.0}};
    static const int N = sizeof(sonyTable) / sizeof(sonyTable[0]);

    int pos = qBound(0, (int)zoomPos, 0x4000);

    double mag = 1.0;
    for (int i = 1; i < N; i++) {
        if (pos <= sonyTable[i].first) {
            double t = (double)(pos - sonyTable[i - 1].first) /
                       (sonyTable[i].first - sonyTable[i - 1].first);

            // Log interpolation for smooth exponential curve
            double logMag0 = std::log(sonyTable[i - 1].second);
            double logMag1 = std::log(sonyTable[i].second);
            mag = std::exp(logMag0 + t * (logMag1 - logMag0));
            break;
        }
    }

    const double hfovWideRad = 46.8 * M_PI / 180.0;
    double hfovRad = 2.0 * std::atan(std::tan(hfovWideRad / 2.0) / mag);

    return hfovRad * 180.0 / M_PI;
}

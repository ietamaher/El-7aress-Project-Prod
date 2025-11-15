// protocols/LrfProtocolParser.cpp
#include "LrfProtocolParser.h"
#include "hardware/messages/LrfMessage.h"
#include <QDebug>

LrfProtocolParser::LrfProtocolParser(QObject* parent) 
    : ProtocolParser(parent) 
{
}

std::vector<MessagePtr> LrfProtocolParser::parse(const QByteArray& rawData) {
    std::vector<MessagePtr> out;
    m_readBuffer.append(rawData);

    while (m_readBuffer.size() >= PACKET_SIZE) {
        // Find valid packet header
        if (static_cast<quint8>(m_readBuffer.at(0)) != FRAME_HEADER) {
            m_readBuffer.remove(0, 1);
            continue;
        }
        if (static_cast<quint8>(m_readBuffer.at(1)) != DeviceCode::LRF) {
            m_readBuffer.remove(0, 1);
            continue;
        }

        QByteArray packet = m_readBuffer.left(PACKET_SIZE);
        m_readBuffer.remove(0, PACKET_SIZE);

        if (verifyChecksum(packet)) {
            if (auto msg = handleResponse(packet)) {
                out.push_back(std::move(msg));
            }
        } else {
            qWarning() << "LRF Checksum mismatch for packet:" << packet.toHex(' ');
        }
    }
    return out;
}

QByteArray LrfProtocolParser::buildCommand(quint8 commandCode, const QByteArray& params) {
    QByteArray packet;
    packet.reserve(PACKET_SIZE);
    packet.append(FRAME_HEADER);
    packet.append(DeviceCode::LRF);

    QByteArray body;
    body.append(commandCode);
    body.append(params);

    // Ensure body is exactly 6 bytes
    if (body.size() < 6) {
        body.append(6 - body.size(), '\0');
    }

    packet.append(body);
    quint8 checksum = calculateChecksum(body);
    packet.append(checksum);
    return packet;
}

quint8 LrfProtocolParser::calculateChecksum(const QByteArray &body) const {
    quint8 sum = 0;
    for (char byte : body) {
        sum += static_cast<quint8>(byte);
    }
    return sum;
}

bool LrfProtocolParser::verifyChecksum(const QByteArray &packet) const {
    if (packet.size() != PACKET_SIZE) return false;
    QByteArray body = packet.mid(2, 6);
    return (static_cast<quint8>(packet.at(8)) == calculateChecksum(body));
}

MessagePtr LrfProtocolParser::handleResponse(const QByteArray &response) {
    quint8 responseCode = static_cast<quint8>(response.at(2));
    
    LrfData data;
    
    switch (responseCode) {
    case 0x01: { // Self-check response
        quint8 status1 = static_cast<quint8>(response.at(3));
        quint8 status0 = static_cast<quint8>(response.at(4));
        data.rawStatusByte = status0;
        data.isFault = (status1 == 0x01);
        data.noEcho = (status0 & 0x08);
        data.laserNotOut = (status0 & 0x10);
        data.isOverTemperature = (status0 & 0x20);
        return std::make_unique<LrfDataMessage>(data);
    }
    case 0x0B: // Fall-through
    case 0x0C: // Fall-through
    case 0x02: // Fall-through
    case 0x04: { // Ranging response
        quint8 status0 = static_cast<quint8>(response.at(3));
        data.rawStatusByte = status0;
        data.isFault = (status0 == 0x01);
        data.noEcho = (status0 & 0x08);
        data.laserNotOut = (status0 & 0x10);
        data.isOverTemperature = (status0 & 0x20);
       quint16 lrfDistance    = (static_cast<quint8>(response.at(6)) << 8) |
                           static_cast<quint8>(response.at(5)) ;
        data.lastDistance = lrfDistance / 100;   // it is in centimeters convert to meeters 
        data.isLastRangingValid = (data.lastDistance > 0 && !data.noEcho && !data.isFault);
        data.pulseCount = static_cast<quint8>(response.at(7));
        return std::make_unique<LrfDataMessage>(data);
    }
    case 0x0A: { // Pulse count response
        quint16 pulse_base = (static_cast<quint8>(response.at(6)) << 8) | 
                            static_cast<quint8>(response.at(5));
        data.laserCount = static_cast<quint32>(pulse_base) * 100;
        return std::make_unique<LrfDataMessage>(data);
    }
    case 0x10: { // Product info response
        quint8 productId = static_cast<quint8>(response.at(3));
        quint8 versionByte = static_cast<quint8>(response.at(4));
        QString versionString = QString("%1.%2")
            .arg((versionByte & 0xF0) >> 4)
            .arg(versionByte & 0x0F);
        return std::make_unique<LrfInfoMessage>(productId, versionString);
    }
    case 0x06: { // Temperature response
        quint8 tempByte = static_cast<quint8>(response.at(4));
        qint8 tempValue = tempByte & 0x7F;
        if (tempByte & 0x80) {
            tempValue = -tempValue;
        }
        data.temperature = tempValue;
        data.isTempValid = true;
        return std::make_unique<LrfDataMessage>(data);
    }
    case 0x05: // Stop ranging - no data
    default:
        return nullptr;
    }
}

#include "RadarProtocolParser.h"
#include "../messages/RadarMessage.h"
#include <QDebug>
#include <QStringList>

RadarProtocolParser::RadarProtocolParser(QObject* parent) : ProtocolParser(parent) {}

std::vector<MessagePtr> RadarProtocolParser::parse(const QByteArray& rawData) {
    std::vector<MessagePtr> messages;

    // Append incoming data to buffer
    m_buffer.append(rawData);

    // NMEA sentences end with <CR><LF> (\r\n)
    while (m_buffer.contains("\r\n")) {
        int endIndex = m_buffer.indexOf("\r\n");
        QByteArray rawSentence = m_buffer.left(endIndex);
        m_buffer.remove(0, endIndex + 2);  // +2 for \r\n

        // NMEA sentences start with '$'
        if (rawSentence.startsWith("$")) {
            if (validateChecksum(rawSentence)) {
                QString sentence = QString(rawSentence).trimmed();
                QStringList parts = sentence.split("*");
                QString dataPart = parts.at(0);

                // Check if it's a RATTM sentence
                if (dataPart.startsWith("$RATTM")) {
                    auto msg = parseRATTM(dataPart.toUtf8());
                    if (msg) {
                        messages.push_back(std::move(msg));
                    }
                }
            } else {
                qWarning() << "NMEA checksum mismatch:" << rawSentence;
            }
        }
    }

    return messages;
}

bool RadarProtocolParser::validateChecksum(const QByteArray& sentence) {
    int asteriskIndex = sentence.indexOf("*");
    if (asteriskIndex == -1 || asteriskIndex + 2 >= sentence.length()) {
        return false;  // No checksum or incomplete checksum
    }

    // Data to checksum: everything between '$' and '*'
    QByteArray dataToChecksum = sentence.mid(1, asteriskIndex - 1);
    quint8 calculatedChecksum = 0;
    for (char c : dataToChecksum) {
        calculatedChecksum ^= static_cast<quint8>(c);
    }

    bool ok;
    quint8 receivedChecksum = sentence.mid(asteriskIndex + 1, 2).toUInt(&ok, 16);

    return ok && (calculatedChecksum == receivedChecksum);
}

MessagePtr RadarProtocolParser::parseRATTM(const QByteArray& sentence) {
    RadarData plot;
    plot.isConnected = true;

    QStringList fields = QString(sentence).split(",");

    // $RATTM,id,bearing,range,T/M,course,speed,...*CS
    if (fields.size() >= 7) {
        plot.id = fields.at(1).toUInt();
        plot.azimuthDegrees = fields.at(2).toFloat();
        plot.rangeMeters = fields.at(3).toFloat() * 1852.0;  // Convert nautical miles to meters
        // fields.at(4) is 'T' or 'M' for True/Magnetic bearing (ignored for now)
        plot.relativeCourseDegrees = fields.at(5).toFloat();
        plot.relativeSpeedMPS = fields.at(6).toFloat() * 0.514444;  // Convert knots to m/s

        return std::make_unique<RadarPlotMessage>(plot);
    } else {
        qWarning() << "Malformed $RATTM sentence:" << sentence;
        return nullptr;
    }
}

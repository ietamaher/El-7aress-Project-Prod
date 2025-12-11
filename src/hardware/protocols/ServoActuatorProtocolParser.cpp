#include "ServoActuatorProtocolParser.h"
#include "../messages/ServoActuatorMessage.h"
#include <QDebug>
#include <QRegularExpression>
#include <cmath>

ServoActuatorProtocolParser::ServoActuatorProtocolParser(QObject* parent)
    : ProtocolParser(parent) {
    // Initialize m_data with defaults
    m_data = ServoActuatorData();
    initializeStatusBitMap();
}

std::vector<MessagePtr> ServoActuatorProtocolParser::parse(const QByteArray& rawData) {
    std::vector<MessagePtr> messages;
    m_readBuffer.append(rawData);

    // Process complete responses (terminated by '\r')
    while (m_readBuffer.contains('\r')) {
        int endIndex = m_readBuffer.indexOf('\r');
        QByteArray responseData = m_readBuffer.left(endIndex).trimmed();
        m_readBuffer.remove(0, endIndex + 1);

        QString response = QString::fromLatin1(responseData);
        if (response.isEmpty()) continue;

        // Validate checksum
        if (!validateChecksum(response)) {
            qWarning() << "ServoActuatorProtocolParser: Checksum mismatch for" << response;
            continue;
        }

        // Extract main response (remove checksum)
        int lastSpaceIndex = response.lastIndexOf(' ');
        if (lastSpaceIndex == -1) continue;
        
        QString mainResponse = response.left(lastSpaceIndex);
        
        // Parse based on response type (ACK or NACK)
        if (mainResponse.startsWith('A')) { // ACK
            // Extract payload: everything after the 'A' prefix
            // Hardware format: "A5000" (no space between A and payload)
            // NOT "A 5000" (space-separated)
            QString dataPart = mainResponse.mid(1).trimmed(); // Skip 'A', get rest

            // ⭐ Update ONLY the relevant field in accumulated m_data based on pending command
            if (m_pendingCommand == "SR") {
                m_data.status = parseStatusRegister(dataPart);
                messages.push_back(std::make_unique<ServoActuatorDataMessage>(m_data));

                // Check for critical faults
                if (m_data.status.isMotorOff) {
                    QStringList criticalFaults;
                    for (const auto& msg : m_data.status.activeStatusMessages) {
                        if (msg.contains("(Latching)") &&
                            (msg.contains("Emergency") || msg.contains("MOTOR OFF"))) {
                            criticalFaults.append(msg);
                        }
                    }
                    if (!criticalFaults.isEmpty()) {
                        messages.push_back(
                            std::make_unique<ServoActuatorCriticalFaultMessage>(criticalFaults));
                    }
                }
            } else if (m_pendingCommand == "AP") {
                m_data.position_mm = sensorCountsToMillimeters(dataPart.toInt());
                messages.push_back(std::make_unique<ServoActuatorDataMessage>(m_data));
            } else if (m_pendingCommand == "VL") {
                m_data.velocity_mm_s = sensorCountsToSpeed(dataPart.toInt());
                messages.push_back(std::make_unique<ServoActuatorDataMessage>(m_data));
            } else if (m_pendingCommand == "TQ") {
                m_data.torque_percent = sensorCountsToTorquePercent(dataPart.toInt());
                messages.push_back(std::make_unique<ServoActuatorDataMessage>(m_data));
            } else if (m_pendingCommand == "RT1") {
                m_data.temperature_c = dataPart.toDouble();
                messages.push_back(std::make_unique<ServoActuatorDataMessage>(m_data));
            } else if (m_pendingCommand == "BV") {
                m_data.busVoltage_v = dataPart.toDouble() / 1000.0;
                messages.push_back(std::make_unique<ServoActuatorDataMessage>(m_data));
            }
            
            // Create ACK message
            messages.push_back(
                std::make_unique<ServoActuatorAckMessage>(m_pendingCommand, dataPart));
                
        } else if (mainResponse.startsWith('N')) { // NACK
            messages.push_back(
                std::make_unique<ServoActuatorNackMessage>(m_pendingCommand, mainResponse));
        }
    }

    return messages;
}

QByteArray ServoActuatorProtocolParser::buildCommand(const QString& command) const {
    QString stringToChecksum = command + " ";
    QString checksum = calculateChecksum(stringToChecksum);
    QString fullCommand = stringToChecksum + checksum + "\r";
    return fullCommand.toLatin1();
}

void ServoActuatorProtocolParser::setPendingCommand(const QString& command) {
    m_pendingCommand = command;
}

ActuatorStatus ServoActuatorProtocolParser::parseStatusRegister(const QString& hexStatus) const {
    ActuatorStatus status;
    
    bool ok;
    quint32 statusValue = hexStatus.toUInt(&ok, 16);
    if (!ok) {
        status.activeStatusMessages.append("Invalid Hex Status Received");
        return status;
    }

    // Note: rawStatusValue was removed from ActuatorStatus structure
    // Status information is conveyed through activeStatusMessages instead

    // Parse each bit
    for (int i = 0; i < 32; ++i) {
        if ((statusValue >> i) & 1) {
            QString message = m_statusBitMap.value(i, QString("Unknown Bit %1").arg(i));
            status.activeStatusMessages.append(message);
            
            if (message.contains("(Latching)")) {
                status.isLatchingFaultActive = true;
                if (i == 3 || i == 31) { // Emergency shutdown or critical config error
                    status.isMotorOff = true;
                }
            }
        }
    }

    return status;
}

QString ServoActuatorProtocolParser::calculateChecksum(const QString& command) const {
    quint16 sum = 0;
    for (const QChar &ch : command) {
        sum += ch.toLatin1();
    }
    quint8 checksum_val = sum % 256;
    return QString("%1").arg(checksum_val, 2, 16, QChar('0')).toUpper();
}

bool ServoActuatorProtocolParser::validateChecksum(const QString& response) const {
    int lastSpaceIndex = response.lastIndexOf(' ');
    if (lastSpaceIndex == -1) return false;

    QString mainResponse = response.left(lastSpaceIndex);
    QString receivedChecksum = response.mid(lastSpaceIndex + 1);
    QString stringToValidate = mainResponse + " ";
    QString calculatedChecksum = calculateChecksum(stringToValidate);

    return receivedChecksum.toUpper() == calculatedChecksum.toUpper();
}

//================================================================================
// UNIT CONVERSION FUNCTIONS
//================================================================================

double ServoActuatorProtocolParser::sensorCountsToMillimeters(int counts) const {
    using namespace ServoActuatorConstants;
    return static_cast<double>(counts - RETRACTED_ENDSTOP_OFFSET) * 
           SCREW_LEAD_MM / COUNTS_PER_REVOLUTION;
}

int ServoActuatorProtocolParser::millimetersToSensorCounts(double millimeters) const {
    using namespace ServoActuatorConstants;
    double counts = (millimeters * COUNTS_PER_REVOLUTION / SCREW_LEAD_MM) + 
                    RETRACTED_ENDSTOP_OFFSET;
    return static_cast<int>(std::round(counts));
}

int ServoActuatorProtocolParser::speedToSensorCounts(double speed_mm_s) const {
    using namespace ServoActuatorConstants;
    double rev_per_sec = speed_mm_s / SCREW_LEAD_MM;
    return static_cast<int>(std::round(rev_per_sec * COUNTS_PER_REVOLUTION));
}

double ServoActuatorProtocolParser::sensorCountsToSpeed(int counts) const {
    using namespace ServoActuatorConstants;
    double rev_per_sec = static_cast<double>(counts) / COUNTS_PER_REVOLUTION;
    return rev_per_sec * SCREW_LEAD_MM;
}

int ServoActuatorProtocolParser::accelToSensorCounts(double accel_mm_s2) const {
    using namespace ServoActuatorConstants;
    double rev_per_sec2 = accel_mm_s2 / SCREW_LEAD_MM;
    return static_cast<int>(std::round(rev_per_sec2 * COUNTS_PER_REVOLUTION));
}

double ServoActuatorProtocolParser::sensorCountsToTorquePercent(int counts) const {
    // Max torque value is 32767 for 100%
    return (static_cast<double>(counts) / 32767.0) * 100.0;
}

int ServoActuatorProtocolParser::torquePercentToSensorCounts(double percent) const {
    return static_cast<int>(std::round((percent / 100.0) * 32767.0));
}

void ServoActuatorProtocolParser::initializeStatusBitMap() {
    m_statusBitMap[0] = "Optically isolated digital input";
    m_statusBitMap[1] = "Relative Humidity > ovHumid";
    m_statusBitMap[2] = "Temperature > ovTemp";
    m_statusBitMap[3] = "Emergency shutdown (Latching)";
    m_statusBitMap[4] = "Supply voltage > upper limit";
    m_statusBitMap[5] = "Motor control is enabled";
    m_statusBitMap[6] = "Trajectory generator is active";
    m_statusBitMap[7] = "Direction is extending";
    m_statusBitMap[8] = "Position < spMin";
    m_statusBitMap[9] = "Position > spMax";
    m_statusBitMap[10] = "Input signal < min value";
    m_statusBitMap[11] = "Input signal > max value";
    m_statusBitMap[12] = "Position error < atTargWin";
    m_statusBitMap[13] = "Position error > ovErrP";
    m_statusBitMap[14] = "Speed > ovSpeed";
    m_statusBitMap[15] = "Torque > ovTorq";
    m_statusBitMap[16] = "Position > posGrtr";
    m_statusBitMap[17] = "Position < posLess";
    m_statusBitMap[18] = "Bridge driver fault indication is active (Latching)";
    m_statusBitMap[19] = "USB is connected";
    m_statusBitMap[20] = "Run against retracted stop (Latching)";
    m_statusBitMap[21] = "Run against extended stop (Latching)";
    m_statusBitMap[22] = "Supply voltage < lower limit (Latching)";
    m_statusBitMap[23] = "Supply voltage > upper limit (Latching)";
    m_statusBitMap[24] = "Bridge driver fault has occurred (Latching)";
    m_statusBitMap[25] = "Bridge current feedback saturated (Latching)";
    m_statusBitMap[26] = "4-20mA input < lower limit";
    m_statusBitMap[27] = "4-20mA output out of range";
    m_statusBitMap[28] = "Internal disk modified (Latching)";
    m_statusBitMap[29] = "HARDWARE.TXT error (Latching)";
    m_statusBitMap[30] = "CONFIG.TXT error (Latching)";
    m_statusBitMap[31] = "Critical config error, MOTOR OFF (Latching)";
}

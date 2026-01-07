#include "Imu3DMGX3ProtocolParser.h"
#include "../messages/ImuMessage.h"
#include <QDebug>
#include <cstring>
#include <QtEndian>

Imu3DMGX3ProtocolParser::Imu3DMGX3ProtocolParser(QObject* parent) : ProtocolParser(parent) {}

std::vector<MessagePtr> Imu3DMGX3ProtocolParser::parse(const QByteArray& rawData) {
    std::vector<MessagePtr> messages;

    // Append new data to buffer
    m_buffer.append(rawData);

    // Process all complete packets in buffer
    while (!m_buffer.isEmpty()) {
        // Need at least 1 byte to identify command
        if (m_buffer.size() < 1) {
            break;
        }

        quint8 command = static_cast<quint8>(m_buffer.at(0));

        // Determine expected packet size based on command byte
        int expectedSize = 0;
        switch (command) {
        case GX3Commands::EULER_ANGLES_AND_RATES:
            expectedSize = PACKET_SIZE_0xCF;
            break;
        case GX3Commands::CAPTURE_GYRO_BIAS:
            expectedSize = PACKET_SIZE_0xCD;
            break;
        case GX3Commands::SAMPLING_SETTINGS:
            expectedSize = PACKET_SIZE_0xDB;
            break;
        case GX3Commands::TEMPERATURES:
            expectedSize = PACKET_SIZE_0xD1;
            break;
        default:
            // Unknown command - try to find next valid command byte
            qWarning() << "Imu3DMGX3Parser: Unknown command byte" << Qt::hex << command;

            // Track consecutive errors - if too many, clear buffer to force resync
            m_consecutiveErrors++;
            if (m_consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                qWarning() << "Imu3DMGX3Parser: Too many consecutive errors ("
                           << m_consecutiveErrors << ") - clearing buffer to resync";
                m_buffer.clear();
                m_consecutiveErrors = 0;
                break;
            }

            m_buffer.remove(0, 1);  // Discard invalid byte
            continue;
        }

        // Wait for complete packet
        if (m_buffer.size() < expectedSize) {
            break;  // Need more data
        }

        // Extract packet
        QByteArray packet = m_buffer.left(expectedSize);
        m_buffer.remove(0, expectedSize);

        // Validate checksum (last 2 bytes)
        QByteArray dataWithoutChecksum = packet.left(packet.size() - 2);
        quint16 receivedChecksum = extractUInt16(packet, packet.size() - 2);
        quint16 calculatedChecksum = calculateChecksum(dataWithoutChecksum);

        if (receivedChecksum != calculatedChecksum) {
            qWarning() << "Imu3DMGX3Parser: Checksum mismatch! Expected" << Qt::hex
                       << calculatedChecksum << "got" << receivedChecksum;

            // Track consecutive errors - if too many, clear buffer to force resync
            m_consecutiveErrors++;
            if (m_consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                qWarning() << "Imu3DMGX3Parser: Too many consecutive errors ("
                           << m_consecutiveErrors << ") - clearing buffer to resync";
                m_buffer.clear();
                m_consecutiveErrors = 0;
                break;
            }
            continue;  // Discard corrupted packet
        }

        // Valid packet received - reset error counter
        m_consecutiveErrors = 0;

        // Parse packet based on command
        MessagePtr msg = nullptr;
        switch (command) {
        case GX3Commands::EULER_ANGLES_AND_RATES:
            msg = parse0xCFPacket(packet);
            break;
        case GX3Commands::CAPTURE_GYRO_BIAS:
            parse0xCDPacket(packet);  // Now properly parses the response
            break;
        case GX3Commands::SAMPLING_SETTINGS:
            parse0xDBPacket(packet);  // Now properly parses the response
            break;
        case GX3Commands::TEMPERATURES:
            parse0xD1Packet(packet);
            break;
        }

        if (msg) {
            messages.push_back(std::move(msg));
        }
    }

    return messages;
}

MessagePtr Imu3DMGX3ProtocolParser::parse0xCFPacket(const QByteArray& packet) {
    if (packet.size() != PACKET_SIZE_0xCF) {
        qWarning() << "Imu3DMGX3Parser: Invalid 0xCF packet size:" << packet.size();
        return nullptr;
    }

    // Verify echo byte
    if (static_cast<quint8>(packet.at(0)) != GX3Commands::EULER_ANGLES_AND_RATES) {
        qWarning() << "Imu3DMGX3Parser: Invalid echo byte in 0xCF packet";
        return nullptr;
    }

    // Parse data fields (all big-endian IEEE 754 floats)
    ImuData data;
    data.isConnected = true;

    // Offset 1: Roll (radians) - CONVERT TO DEGREES
    float rollRad = extractFloat(packet, 1);
    data.rollDeg = rollRad * (180.0 / M_PI);

    // Offset 5: Pitch (radians) - CONVERT TO DEGREES
    float pitchRad = extractFloat(packet, 5);
    data.pitchDeg = pitchRad * (180.0 / M_PI);

    // Offset 9: Yaw (radians, range: -π to π) - CONVERT TO DEGREES (0° to 360°)
    float yawRad = extractFloat(packet, 9);
    data.yawDeg = yawRad * (180.0 / M_PI);
    // Normalize to [0, 360) range for display and gimbal control
    if (data.yawDeg < 0.0) {
        data.yawDeg += 360.0;
    }

    // Offset 13: Roll Rate (rad/s) - CONVERT TO DEG/S
    float rollRateRad = extractFloat(packet, 13);
    data.angRateX_dps = rollRateRad * (180.0 / M_PI);

    // Offset 17: Pitch Rate (rad/s) - CONVERT TO DEG/S
    float pitchRateRad = extractFloat(packet, 17);
    data.angRateY_dps = pitchRateRad * (180.0 / M_PI);

    // Offset 21: Yaw Rate (rad/s) - CONVERT TO DEG/S
    float yawRateRad = extractFloat(packet, 21);
    data.angRateZ_dps = yawRateRad * (180.0 / M_PI);

    // Offset 25: Timer (32-bit unsigned, ticks at 62.5µs)
    // quint32 timerTicks = extractUInt32(packet, 25);

    // Acceleration data not provided by 0xCF command
    data.accelX_g = 0.0;
    data.accelY_g = 0.0;
    data.accelZ_g = 0.0;

    // Temperature from last 0xD1 query
    data.temperature = m_lastTemperature;

    // Validate data ranges (sanity checks)
    if (std::isnan(data.rollDeg) || std::isnan(data.pitchDeg) || std::isnan(data.yawDeg) ||
        std::isnan(data.angRateX_dps) || std::isnan(data.angRateY_dps) ||
        std::isnan(data.angRateZ_dps)) {
        qWarning() << "Imu3DMGX3Parser: NaN detected in 0xCF data!";
        return nullptr;
    }

    return std::make_unique<ImuDataMessage>(data);
}

void Imu3DMGX3ProtocolParser::parse0xD1Packet(const QByteArray& packet) {
    if (packet.size() != PACKET_SIZE_0xD1) {
        qWarning() << "Imu3DMGX3Parser: Invalid 0xD1 packet size:" << packet.size();
        return;
    }

    // Verify echo byte
    if (static_cast<quint8>(packet.at(0)) != GX3Commands::TEMPERATURES) {
        qWarning() << "Imu3DMGX3Parser: Invalid echo byte in 0xD1 packet";
        return;
    }

    // Parse temperature fields (all big-endian IEEE 754 floats)
    // Offset 1: Magnetometer temperature (°C)
    float magTemp = extractFloat(packet, 1);

    // Offset 5: Accelerometer temperature (°C)
    float accelTemp = extractFloat(packet, 5);

    // Offset 9: Gyro X temperature (°C)
    float gyroXTemp = extractFloat(packet, 9);

    // Offset 13: Gyro Y temperature (°C)
    float gyroYTemp = extractFloat(packet, 13);

    // Offset 17: Gyro Z temperature (°C)
    float gyroZTemp = extractFloat(packet, 17);

    // Offset 21: Timer (32-bit unsigned, ticks at 62.5µs)
    // quint32 timerTicks = extractUInt32(packet, 21);

    // Calculate average temperature across all sensors
    m_lastTemperature = (magTemp + accelTemp + gyroXTemp + gyroYTemp + gyroZTemp) / 5.0;

    qDebug() << "Imu3DMGX3Parser: Temperatures -" << "Mag:" << QString::number(magTemp, 'f', 1)
             << "°C" << "Accel:" << QString::number(accelTemp, 'f', 1) << "°C"
             << "GyroX:" << QString::number(gyroXTemp, 'f', 1) << "°C"
             << "GyroY:" << QString::number(gyroYTemp, 'f', 1) << "°C"
             << "GyroZ:" << QString::number(gyroZTemp, 'f', 1) << "°C"
             << "Avg:" << QString::number(m_lastTemperature, 'f', 1) << "°C";
}

float Imu3DMGX3ProtocolParser::extractFloat(const QByteArray& data, int offset) const {
    if (offset + 4 > data.size()) {
        return 0.0f;
    }

    // Extract 4 bytes as big-endian
    quint32 rawBits =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + offset));

    // Reinterpret bits as IEEE 754 float
    float value;
    std::memcpy(&value, &rawBits, sizeof(float));

    return value;
}

quint32 Imu3DMGX3ProtocolParser::extractUInt32(const QByteArray& data, int offset) const {
    if (offset + 4 > data.size()) {
        return 0;
    }

    return qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

quint16 Imu3DMGX3ProtocolParser::extractUInt16(const QByteArray& data, int offset) const {
    if (offset + 2 > data.size()) {
        return 0;
    }

    return qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

quint16 Imu3DMGX3ProtocolParser::calculateChecksum(const QByteArray& data) {
    quint16 checksum = 0;
    for (char byte : data) {
        checksum += static_cast<quint8>(byte);
    }
    return checksum;
}

QByteArray Imu3DMGX3ProtocolParser::createContinuousModeCommand() {
    // Command format: 0xC4 <command_byte>
    // This sets continuous mode to output the specified command continuously
    QByteArray cmd;
    cmd.append(static_cast<char>(GX3Commands::SET_CONTINUOUS_MODE));
    cmd.append(static_cast<char>(GX3Commands::EULER_ANGLES_AND_RATES));
    return cmd;
}

QByteArray Imu3DMGX3ProtocolParser::createStopContinuousModeCommand() {
    QByteArray cmd;
    cmd.append(static_cast<char>(GX3Commands::STOP_CONTINUOUS_MODE));
    return cmd;
}

QByteArray Imu3DMGX3ProtocolParser::createCaptureGyroBiasCommand(quint16 samplingTimeMs) {
    // 0xCD - Full command is 5 bytes (device must be stationary):
    // [0xCD, 0xC1, 0x29, SamplingTime_MSB, SamplingTime_LSB]
    QByteArray cmd;
    cmd.append(static_cast<char>(GX3Commands::CAPTURE_GYRO_BIAS));  // 0xCD
    cmd.append(static_cast<char>(0xC1));                            // Confirmation byte 1
    cmd.append(static_cast<char>(0x29));                            // Confirmation byte 2

    // Sampling time in milliseconds (big-endian, recommended: 10000-30000 ms)
    cmd.append(static_cast<char>((samplingTimeMs >> 8) & 0xFF));  // MSB
    cmd.append(static_cast<char>(samplingTimeMs & 0xFF));         // LSB

    return cmd;
}

QByteArray Imu3DMGX3ProtocolParser::createSamplingSettingsCommand(quint8 function,
                                                                  quint16 decimation,
                                                                  quint16 flags) {
    // 0xDB - Full command is 21 bytes:
    // [0xDB, 0xA8, 0xB9, Function, Decimation(2), Flags(2),
    //  GyroAccelFilter(1), MagFilter(1), UpComp(2), NorthComp(2),
    //  MagPower(1), Reserved(5)]

    QByteArray cmd;
    cmd.append(static_cast<char>(GX3Commands::SAMPLING_SETTINGS));  // 0xDB
    cmd.append(static_cast<char>(0xA8));                            // Confirmation byte 1
    cmd.append(static_cast<char>(0xB9));                            // Confirmation byte 2

    // Function selector (0=Read, 1=Write, 2=Write+Save, 3=Write no reply)
    cmd.append(static_cast<char>(function));

    // Data Rate Decimation (big-endian): 1000/decimation = output rate Hz
    // Examples: 1=1000Hz, 2=500Hz, 4=250Hz, 10=100Hz, 20=50Hz
    cmd.append(static_cast<char>((decimation >> 8) & 0xFF));
    cmd.append(static_cast<char>(decimation & 0xFF));

    // Data Conditioning Flags (big-endian)
    // Bit 0: Calculate orientation (1)
    // Bit 1: Enable Coning&Sculling (1)
    // Default: 0x0003 (both enabled)
    cmd.append(static_cast<char>((flags >> 8) & 0xFF));
    cmd.append(static_cast<char>(flags & 0xFF));

    // Gyro/Accel digital filter window size (default: 15)
    // First null at 1000/size Hz
    cmd.append(static_cast<char>(15));

    // Mag digital filter window size (default: 17)
    cmd.append(static_cast<char>(17));

    // Up compensation in seconds (default: 10)
    // Controls gravity vector correction rate
    quint16 upComp = 10;
    cmd.append(static_cast<char>((upComp >> 8) & 0xFF));
    cmd.append(static_cast<char>(upComp & 0xFF));

    // North compensation in seconds (default: 10)
    // Controls magnetometer correction rate for yaw
    quint16 northComp = 10;
    cmd.append(static_cast<char>((northComp >> 8) & 0xFF));
    cmd.append(static_cast<char>(northComp & 0xFF));

    // Mag power/bandwidth setting (0=highest power/bandwidth, 1=lower power)
    cmd.append(static_cast<char>(0x00));

    // Reserved (5 bytes) - must be 0x00
    for (int i = 0; i < 5; ++i) {
        cmd.append(static_cast<char>(0x00));
    }

    return cmd;
}

QByteArray Imu3DMGX3ProtocolParser::createReadTemperaturesCommand() {
    // 0xD1 - Single byte command to query sensor temperatures
    QByteArray cmd;
    cmd.append(static_cast<char>(GX3Commands::TEMPERATURES));
    return cmd;
}

void Imu3DMGX3ProtocolParser::parse0xCDPacket(const QByteArray& packet) {
    if (packet.size() != PACKET_SIZE_0xCD) {
        qWarning() << "Imu3DMGX3Parser: Invalid 0xCD packet size:" << packet.size();
        return;
    }

    // Verify echo byte
    if (static_cast<quint8>(packet.at(0)) != GX3Commands::CAPTURE_GYRO_BIAS) {
        qWarning() << "Imu3DMGX3Parser: Invalid echo byte in 0xCD packet";
        return;
    }

    // Parse gyro bias values (all big-endian IEEE 754 floats)
    float gyroBiasX = extractFloat(packet, 1);  // Offset 1
    float gyroBiasY = extractFloat(packet, 5);  // Offset 5
    float gyroBiasZ = extractFloat(packet, 9);  // Offset 9
    // quint32 timer = extractUInt32(packet, 13);  // Offset 13

    qDebug() << "Imu3DMGX3Parser: Gyro bias captured successfully -"
             << "X:" << QString::number(gyroBiasX, 'f', 4) << "deg/s"
             << "Y:" << QString::number(gyroBiasY, 'f', 4) << "deg/s"
             << "Z:" << QString::number(gyroBiasZ, 'f', 4) << "deg/s";
}

void Imu3DMGX3ProtocolParser::parse0xDBPacket(const QByteArray& packet) {
    if (packet.size() != PACKET_SIZE_0xDB) {
        qWarning() << "Imu3DMGX3Parser: Invalid 0xDB packet size:" << packet.size();
        return;
    }

    // Verify echo byte
    if (static_cast<quint8>(packet.at(0)) != GX3Commands::SAMPLING_SETTINGS) {
        qWarning() << "Imu3DMGX3Parser: Invalid echo byte in 0xDB packet";
        return;
    }

    // Parse sampling settings response
    quint16 decimation = extractUInt16(packet, 1);
    quint16 flags = extractUInt16(packet, 3);
    quint8 gyroAccelFilter = static_cast<quint8>(packet.at(5));
    quint8 magFilter = static_cast<quint8>(packet.at(6));
    quint16 upComp = extractUInt16(packet, 7);
    quint16 northComp = extractUInt16(packet, 9);

    float dataRateHz = 1000.0f / decimation;

    qDebug() << "Imu3DMGX3Parser: Sampling settings confirmed -"
             << "Rate:" << QString::number(dataRateHz, 'f', 1) << "Hz"
             << "Flags: 0x" + QString::number(flags, 16).toUpper()
             << "Filters: Gyro/Accel=" << gyroAccelFilter << "Mag=" << magFilter
             << "Comp: Up=" << upComp << "s, North=" << northComp << "s";
}

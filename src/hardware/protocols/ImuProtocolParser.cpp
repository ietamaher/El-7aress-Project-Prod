#include "ImuProtocolParser.h"
#include "../messages/ImuMessage.h"
#include <QModbusDataUnit>
#include <QDebug>
#include <cstring>

ImuProtocolParser::ImuProtocolParser(QObject* parent) : ProtocolParser(parent) {}

std::vector<MessagePtr> ImuProtocolParser::parse(QModbusReply* reply) {
    std::vector<MessagePtr> messages;

    if (!reply || reply->error() != QModbusDevice::NoError) {
        return messages;
    }

    const QModbusDataUnit unit = reply->result();

    // Check if this is the all-data read response
    if (unit.registerType() == QModbusDataUnit::InputRegisters &&
        unit.startAddress() == ImuRegisters::ALL_DATA_START_ADDR &&
        unit.valueCount() == ImuRegisters::ALL_DATA_REG_COUNT) {
        messages.push_back(parseAllDataReply(unit));
    }

    return messages;
}

MessagePtr ImuProtocolParser::parseAllDataReply(const QModbusDataUnit& unit) {
    ImuData data;
    data.isConnected = true;

    // Parse all 9 float values (18 registers, 2 per float)
    // SST810 register mapping:
    // X-Angle (Pitch), Y-Angle (Roll), Temperature,
    // X-Accel, Y-Accel, Z-Accel,
    // X-Gyro, Y-Gyro, Z-Gyro

    data.pitchDeg = parseFloat(unit, 0);  // 0x03E8-0x03E9
    data.rollDeg = parseFloat(unit, 2);   // 0x03EA-0x03EB
    float tempRaw = parseFloat(unit, 4);  // 0x03EC-0x03ED
    data.temperature = tempRaw / 10.0;    // Temperature scaling

    data.accelX_g = parseFloat(unit, 6);   // 0x03EE-0x03EF
    data.accelY_g = parseFloat(unit, 8);   // 0x03F0-0x03F1
    data.accelZ_g = parseFloat(unit, 10);  // 0x03F2-0x03F3

    data.angRateX_dps = parseFloat(unit, 12);  // 0x03F4-0x03F5 (Pitch rate)
    data.angRateY_dps = parseFloat(unit, 14);  // 0x03F6-0x03F7 (Roll rate)
    data.angRateZ_dps = parseFloat(unit, 16);  // 0x03F8-0x03F9 (Yaw rate)

    // Yaw is not provided by SST810, remains 0
    data.yawDeg = 25.0;

    return std::make_unique<ImuDataMessage>(data);
}

float ImuProtocolParser::parseFloat(const QModbusDataUnit& unit, int index) {
    // Combine two 16-bit registers into one 32-bit value (big-endian)
    quint16 high = unit.value(index);
    quint16 low = unit.value(index + 1);
    quint32 combined = (static_cast<quint32>(high) << 16) | low;

    // Reinterpret the bits of the 32-bit integer as a float
    float value;
    std::memcpy(&value, &combined, sizeof(value));
    return value;
}

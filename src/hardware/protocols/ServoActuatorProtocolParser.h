#pragma once
#include "../interfaces/ProtocolParser.h"
#include "../data/DataTypes.h"
#include <QMap>

//================================================================================
// SERVO ACTUATOR PROTOCOL PARSER (Serial ASCII-based)
//================================================================================

/**
 * @brief Physical constants for servo actuator
 */
namespace ServoActuatorConstants {
constexpr double SCREW_LEAD_MM = 3.175;
constexpr int COUNTS_PER_REVOLUTION = 1024;
constexpr int RETRACTED_ENDSTOP_OFFSET = 1024;
}  // namespace ServoActuatorConstants

/**
 * @brief Parser for serial ASCII-based servo actuator protocol
 *
 * Converts ASCII command responses into typed Message objects.
 * Handles checksumming, ACK/NACK responses, and data parsing.
 *
 * IMPORTANT: Maintains accumulated state in m_data since servo actuator data comes
 * from multiple separate command responses (SR, AP, VL, TQ, RT1, BV).
 */
class ServoActuatorProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit ServoActuatorProtocolParser(QObject* parent = nullptr);
    ~ServoActuatorProtocolParser() override = default;

    // Parse incoming raw data stream
    std::vector<MessagePtr> parse(const QByteArray& rawData) override;

    // Build command with checksum
    QByteArray buildCommand(const QString& command) const;

    // Set the pending command for response routing
    void setPendingCommand(const QString& command);

    // Unit conversion functions
    double sensorCountsToMillimeters(int counts) const;
    int millimetersToSensorCounts(double millimeters) const;
    int speedToSensorCounts(double speed_mm_s) const;
    double sensorCountsToSpeed(int counts) const;
    int accelToSensorCounts(double accel_mm_s2) const;
    double sensorCountsToTorquePercent(int counts) const;
    int torquePercentToSensorCounts(double percent) const;

private:
    // Parse status register into ActuatorStatus structure
    ActuatorStatus parseStatusRegister(const QString& hexStatus) const;

    // Calculate checksum for command
    QString calculateChecksum(const QString& command) const;

    // Validate received checksum
    bool validateChecksum(const QString& response) const;

    // Status bit mapping
    void initializeStatusBitMap();
    QMap<int, QString> m_statusBitMap;

    // Read buffer for accumulating incoming data
    QByteArray m_readBuffer;

    // Track current pending command for response routing
    QString m_pendingCommand;

    // ⭐ Accumulated data state (persists between command responses)
    ServoActuatorData m_data;
};

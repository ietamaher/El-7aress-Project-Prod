#pragma once
#include "../interfaces/ProtocolParser.h"
#include <QByteArray>

//================================================================================
// 3DM-GX3-25 MICROSTRAIN AHRS PROTOCOL PARSER (Serial Binary)
//================================================================================

/**
 * @brief Command bytes for 3DM-GX3-25 AHRS
 */
namespace GX3Commands {
constexpr quint8 EULER_ANGLES_AND_RATES = 0xCF;  // Primary command for gimbal control
constexpr quint8 CAPTURE_GYRO_BIAS = 0xCD;       // Gyro bias calibration
constexpr quint8 SET_CONTINUOUS_MODE = 0xC4;     // Enable continuous streaming
constexpr quint8 STOP_CONTINUOUS_MODE = 0xFA;    // Stop streaming (no reply)
constexpr quint8 SAMPLING_SETTINGS = 0xDB;       // Configure sampling rate
constexpr quint8 TEMPERATURES = 0xD1;            // Read sensor temperatures (status)
constexpr quint8 MODE_COMMAND = 0xD4;            // Set device mode
constexpr quint8 DEVICE_RESET = 0xFE;            // Reset device (no reply)
constexpr quint8 READ_FIRMWARE_VERSION = 0xE9;   // Get firmware version
constexpr quint8 GYRO_STABILIZED_DATA = 0xD2;    // Alternative: gyro-stabilized outputs
}  // namespace GX3Commands

/**
 * @brief Parser for 3DM-GX3-25 MicroStrain AHRS serial binary protocol
 *
 * This parser handles the single-byte command protocol used by the 3DM-GX3-25.
 * Primary command: 0xCF (Euler Angles and Angular Rates)
 *
 * Packet format for 0xCF (31 bytes):
 * - Echo (1 byte): 0xCF
 * - Roll (4 bytes): IEEE 754 float, degrees
 * - Pitch (4 bytes): IEEE 754 float, degrees
 * - Yaw (4 bytes): IEEE 754 float, degrees (magnetic heading)
 * - Roll Rate (4 bytes): IEEE 754 float, deg/s
 * - Pitch Rate (4 bytes): IEEE 754 float, deg/s
 * - Yaw Rate (4 bytes): IEEE 754 float, deg/s
 * - Timer (4 bytes): 32-bit unsigned, ticks (62.5 µs each)
 * - Checksum (2 bytes): Big-endian sum of all bytes
 */
class Imu3DMGX3ProtocolParser : public ProtocolParser {
    Q_OBJECT

public:
    explicit Imu3DMGX3ProtocolParser(QObject* parent = nullptr);
    ~Imu3DMGX3ProtocolParser() override = default;

    /**
     * @brief Parses incoming serial data stream into typed Message objects
     * @param rawData Byte stream from serial port
     * @return Vector of parsed messages (ImuDataMessage)
     */
    std::vector<MessagePtr> parse(const QByteArray& rawData) override;

    /**
     * @brief Not used for serial protocol (Modbus-specific)
     */
    std::vector<MessagePtr> parse(QModbusReply* /*reply*/) override {
        return {};
    }

    /**
     * @brief Creates command to enter continuous mode with 0xCF data
     * @return 2-byte command: {0xC4, 0xCF}
     */
    static QByteArray createContinuousModeCommand();

    /**
     * @brief Creates command to stop continuous mode
     * @return 1-byte command: {0xFA}
     */
    static QByteArray createStopContinuousModeCommand();

    /**
     * @brief Creates command to capture gyro bias (device must be stationary)
     * @param samplingTimeMs Sampling duration in milliseconds (recommended: 10000-30000)
     * @return 5-byte command: {0xCD, 0xC1, 0x29, TimeH, TimeL}
     */
    static QByteArray createCaptureGyroBiasCommand(quint16 samplingTimeMs = 10000);

    /**
     * @brief Creates command to set sampling rate and filters
     * @param function 0=Read only, 1=Write, 2=Write+Save to EEPROM, 3=Write no reply
     * @param decimation Data rate decimation (1000/decimation = Hz, e.g., 10 = 100Hz)
     * @param flags Data conditioning flags (default: 0x0003 = Orient+ConingAndSculling)
     * @return 21-byte command with all parameters
     */
    static QByteArray createSamplingSettingsCommand(quint8 function = 1, quint16 decimation = 10,
                                                    quint16 flags = 0x0003);

    /**
     * @brief Creates command to read sensor temperatures
     * @return 1-byte command: {0xD1}
     */
    static QByteArray createReadTemperaturesCommand();

    /**
     * @brief Calculates 16-bit checksum for packet validation
     * @param data Packet data (excluding checksum bytes)
     * @return 16-bit checksum (sum of all bytes)
     */
    static quint16 calculateChecksum(const QByteArray& data);

signals:
    /**
     * @brief Emitted when gyro bias capture completes
     * @param biasX X-axis gyro bias (deg/s)
     * @param biasY Y-axis gyro bias (deg/s)
     * @param biasZ Z-axis gyro bias (deg/s)
     */
    void gyroBiasCaptured(float biasX, float biasY, float biasZ);

    /**
     * @brief Emitted when sampling settings are confirmed
     * @param decimation Current decimation value
     * @param dataRateHz Actual data rate in Hz
     */
    void samplingSettingsConfirmed(quint16 decimation, float dataRateHz);

    /**
     * @brief Emitted when temperature data is received
     * @param avgTemperature Average temperature across all sensors (°C)
     */
    void temperatureReceived(double avgTemperature);

private:
    /**
     * @brief Parses a complete 0xCF packet (31 bytes)
     * @param packet Complete packet data
     * @return ImuDataMessage or nullptr if invalid
     */
    MessagePtr parse0xCFPacket(const QByteArray& packet);

    /**
     * @brief Parses a complete 0xCD gyro bias response (19 bytes)
     * @param packet Complete packet data
     */
    void parse0xCDPacket(const QByteArray& packet);

    /**
     * @brief Parses a complete 0xDB sampling settings response (19 bytes)
     * @param packet Complete packet data
     */
    void parse0xDBPacket(const QByteArray& packet);

    /**
     * @brief Parses a complete 0xD1 temperature packet (27 bytes)
     * @param packet Complete packet data
     */
    void parse0xD1Packet(const QByteArray& packet);

    /**
     * @brief Extracts IEEE 754 float from byte array (big-endian)
     */
    float extractFloat(const QByteArray& data, int offset) const;

    /**
     * @brief Extracts 32-bit unsigned integer (big-endian)
     */
    quint32 extractUInt32(const QByteArray& data, int offset) const;

    /**
     * @brief Extracts 16-bit unsigned integer (big-endian)
     */
    quint16 extractUInt16(const QByteArray& data, int offset) const;

    // Buffer for accumulating partial packets
    QByteArray m_buffer;

    // Temperature cache (updated periodically from 0xD1 queries)
    double m_lastTemperature = 25.0;  // Average of all sensor temps

    // Error tracking for parser resilience
    int m_consecutiveErrors = 0;
    static constexpr int MAX_CONSECUTIVE_ERRORS = 10;

    // Expected packet sizes for different response packets
    static constexpr int PACKET_SIZE_0xCF = 31;  // Euler Angles + Rates
    static constexpr int PACKET_SIZE_0xCD = 19;  // Gyro Bias response (3 floats + timer)
    static constexpr int PACKET_SIZE_0xDB = 19;  // Sampling Settings response
    static constexpr int PACKET_SIZE_0xD1 = 27;  // Temperatures (5 floats + timer)

public:
    /**
     * @brief Gets the last read temperature (averaged across sensors)
     * @return Temperature in degrees Celsius
     */
    double lastTemperature() const {
        return m_lastTemperature;
    }
};

#ifndef DATATYPES_H
#define DATATYPES_H

#include <QtCore>
#include <QStringList>
#include <cmath>  // For std::abs in epsilon-based comparisons

// ============================================================================
// CAMERA DATA STRUCTURES
// ============================================================================

/**
 * @brief Day camera data structure (Pelco-D protocol)
 */
struct DayCameraData {
    bool isConnected = false;
    bool errorState = false;
    quint8 cameraStatus = 0;

    // Zoom control
    bool zoomMovingIn = false;
    bool zoomMovingOut = false;
    quint16 zoomPosition = 65535;   // 14-bit max for VISCA

    // Focus control
    bool autofocusEnabled = true;
    quint16 focusPosition = 65535;  // 12-bit max

    // Field of view (Sony FCB-EV7520A: 1280×720 native → 1024×768 cropped, NOT square!)
    float currentHFOV = 11.0;  // Horizontal FOV (2.3° - 63.7° zoom range)
    float currentVFOV = 11.0;  // Vertical FOV (calculated from HFOV and aspect ratio)

    bool operator!=(const DayCameraData &other) const {
        return (isConnected != other.isConnected ||
                errorState != other.errorState ||
                cameraStatus != other.cameraStatus ||
                zoomMovingIn != other.zoomMovingIn ||
                zoomMovingOut != other.zoomMovingOut ||
                zoomPosition != other.zoomPosition ||
                autofocusEnabled != other.autofocusEnabled ||
                focusPosition != other.focusPosition ||
                !qFuzzyCompare(currentHFOV, other.currentHFOV) ||
                !qFuzzyCompare(currentVFOV, other.currentVFOV));
    }
};

/**
 * @brief Night camera data structure (TAU2 protocol)
 */
struct NightCameraData {
    bool isConnected = false;
    quint8 errorState = 0x00;
    bool ffcInProgress = false;
    bool digitalZoomEnabled = false;
    quint8 digitalZoomLevel = 0;
    double currentHFOV = 10.4;
    double currentVFOV = 8.0;  // FLIR TAU 2 640×512: Wide 10.4°×8°, Narrow 5.2°×4°
    quint16 videoMode = 0;
    quint8 lut = 0;
    quint8 cameraStatus = 0;

    // Temperature (0x20 READ_TEMP_SENSOR)
    qint16 fpaTemperature = 0;  // Celsius × 10 (e.g., 325 = 32.5°C)

    // Pan/Tilt for zoom navigation (0x70 PAN_AND_TILT)
    qint16 panPosition = 0;   // -82 to +82
    qint16 tiltPosition = 0;  // -68 to +68

    bool operator!=(const NightCameraData &other) const {
        return (isConnected != other.isConnected ||
                errorState != other.errorState ||
                ffcInProgress != other.ffcInProgress ||
                digitalZoomEnabled != other.digitalZoomEnabled ||
                digitalZoomLevel != other.digitalZoomLevel ||
                !qFuzzyCompare(currentHFOV, other.currentHFOV) ||
                !qFuzzyCompare(currentVFOV, other.currentVFOV) ||
                videoMode != other.videoMode ||
                cameraStatus != other.cameraStatus ||
                fpaTemperature != other.fpaTemperature ||
                panPosition != other.panPosition ||
                tiltPosition != other.tiltPosition);
    }
};

// ============================================================================
// SENSOR DATA STRUCTURES
// ============================================================================

/**
 * @brief Radar target tracking data structure (NMEA RATTM)
 */
struct RadarData {
    bool isConnected = false;
    quint32 id = 0;
    float azimuthDegrees = 0.0f;
    float rangeMeters = 0.0f;
    float relativeCourseDegrees = 0.0f;
    float relativeSpeedMPS = 0.0f;

    bool operator==(const RadarData &other) const {
        return (isConnected == other.isConnected &&
                id == other.id &&
                qFuzzyCompare(azimuthDegrees, other.azimuthDegrees) &&
                qFuzzyCompare(rangeMeters, other.rangeMeters) &&
                qFuzzyCompare(relativeCourseDegrees, other.relativeCourseDegrees) &&
                qFuzzyCompare(relativeSpeedMPS, other.relativeSpeedMPS));
    }

    bool operator!=(const RadarData &other) const {
        return !(*this == other);
    }
};


/**
 * @brief Laser Range Finder data structure
 */
struct LrfData {
    bool isConnected = false;
    quint16 lastDistance = 0;
    bool isLastRangingValid = false;
    quint8 pulseCount = 0;
    quint8 rawStatusByte = 0;
    bool isFault = false;
    bool noEcho = false;
    bool laserNotOut = false;
    bool isOverTemperature = false;
    bool isTempValid = false;
    qint8 temperature = 0;
    quint32 laserCount = 0;

    bool operator!=(const LrfData &other) const {
        return (isConnected != other.isConnected ||
                lastDistance != other.lastDistance ||
                isLastRangingValid != other.isLastRangingValid ||
                pulseCount != other.pulseCount ||
                rawStatusByte != other.rawStatusByte ||
                isFault != other.isFault ||
                noEcho != other.noEcho ||
                laserNotOut != other.laserNotOut ||
                isOverTemperature != other.isOverTemperature ||
                isTempValid != other.isTempValid ||
                temperature != other.temperature ||
                laserCount != other.laserCount);
    }
};

/**
 * @brief IMU/Inclinometer data structure
 */
struct ImuData {
    bool isConnected = false;

    // Processed angles (from Kalman filter)
    double rollDeg = 0.0;
    double pitchDeg = 0.0;
    double yawDeg = 0.0;

    // Physical state
    double temperature = 0.0;

    // Raw IMU data
    double accelX_g = 0.0;
    double accelY_g = 0.0;
    double accelZ_g = 0.0;
    double angRateX_dps = 0.0;
    double angRateY_dps = 0.0;
    double angRateZ_dps = 0.0;

    bool operator!=(const ImuData &other) const {
        // Use epsilon-based comparison for floating-point values to avoid signal flooding
        // due to insignificant precision differences
        const double ANGLE_EPSILON = 0.01;      // 0.01° = 36 arcseconds (very precise)
        const double TEMP_EPSILON = 0.1;        // 0.1°C
        const double ACCEL_EPSILON = 0.001;     // 0.001g (very sensitive)
        const double GYRO_EPSILON = 0.01;       // 0.01 deg/s

        return (isConnected != other.isConnected ||
                std::abs(rollDeg - other.rollDeg) > ANGLE_EPSILON ||
                std::abs(pitchDeg - other.pitchDeg) > ANGLE_EPSILON ||
                std::abs(yawDeg - other.yawDeg) > ANGLE_EPSILON ||
                std::abs(temperature - other.temperature) > TEMP_EPSILON ||
                std::abs(accelX_g - other.accelX_g) > ACCEL_EPSILON ||
                std::abs(accelY_g - other.accelY_g) > ACCEL_EPSILON ||
                std::abs(accelZ_g - other.accelZ_g) > ACCEL_EPSILON ||
                std::abs(angRateX_dps - other.angRateX_dps) > GYRO_EPSILON ||
                std::abs(angRateY_dps - other.angRateY_dps) > GYRO_EPSILON ||
                std::abs(angRateZ_dps - other.angRateZ_dps) > GYRO_EPSILON);
    }
};

// ============================================================================
// SERVO/ACTUATOR DATA STRUCTURES
// ============================================================================

/**
 * @brief Servo driver data structure (Modbus)
 */
struct ServoDriverData {
    bool isConnected = false;
    float position = 0.0f;
    float rpm = 0.0f;
    float torque = 0.0f;
    float motorTemp = 0.0f;
    float driverTemp = 0.0f;
    bool fault = false;


    bool operator!=(const ServoDriverData &other) const {
        // Use epsilon-based comparison for floating-point values to avoid signal flooding
        // due to insignificant precision differences
        const float POSITION_EPSILON = 1.0f;    // 1 encoder count (≈0.0016°, very precise)
        const float RPM_EPSILON = 1.0f;         // 1 RPM
        const float TORQUE_EPSILON = 0.1f;      // 0.1% torque
        const float TEMP_EPSILON = 0.1f;        // 0.1°C

        return (isConnected != other.isConnected ||
                std::abs(position - other.position) > POSITION_EPSILON ||
                std::abs(rpm - other.rpm) > RPM_EPSILON ||
                std::abs(torque - other.torque) > TORQUE_EPSILON ||
                std::abs(motorTemp - other.motorTemp) > TEMP_EPSILON ||
                std::abs(driverTemp - other.driverTemp) > TEMP_EPSILON ||
                fault != other.fault);
    }
};

/**
 * @brief Servo actuator status structure
 */
struct ActuatorStatus {
    bool isMotorOff = false;
    bool isLatchingFaultActive = false;
    QStringList activeStatusMessages;
    //quint32 ActuatorStatus = 0;

    bool operator!=(const ActuatorStatus &other) const {
        return (isMotorOff != other.isMotorOff ||
                isLatchingFaultActive != other.isLatchingFaultActive ||
                activeStatusMessages != other.activeStatusMessages);
    }
};

/**
 * @brief Servo actuator data structure
 */
struct ServoActuatorData {
    bool isConnected = false;
    double position_mm = 0.0;
    double velocity_mm_s = 0.0;
    double temperature_c = 0.0;
    double busVoltage_v = 0.0;
    double torque_percent = 0.0;
    ActuatorStatus status;

    bool operator!=(const ServoActuatorData &other) const {
        return (isConnected != other.isConnected ||
                !qFuzzyCompare(position_mm, other.position_mm) ||
                !qFuzzyCompare(velocity_mm_s, other.velocity_mm_s) ||
                !qFuzzyCompare(temperature_c, other.temperature_c) ||
                !qFuzzyCompare(busVoltage_v, other.busVoltage_v) ||
                !qFuzzyCompare(torque_percent, other.torque_percent) ||
                status != other.status);
    }
};

// ============================================================================
// PLC DATA STRUCTURES
// ============================================================================

/**
 * @brief PLC21 panel data structure
 */
struct Plc21PanelData {
    bool isConnected = false;

    // Digital inputs
    bool armGunSW = false;
    bool loadAmmunitionSW = false;
    bool enableStationSW = false;
    bool homePositionSW = false;
    bool enableStabilizationSW = false;
    bool authorizeSw = false;
    bool switchCameraSW = false;
    bool menuUpSW = false;
    bool menuDownSW = false;
    bool menuValSw = false;

    // Analog inputs
    int speedSW = 50;
    int fireMode = 99; // To force update on first read
    int panelTemperature = 0;

    bool operator!=(const Plc21PanelData &other) const {
        return (isConnected != other.isConnected ||
                armGunSW != other.armGunSW ||
                loadAmmunitionSW != other.loadAmmunitionSW ||
                enableStationSW != other.enableStationSW ||
                homePositionSW != other.homePositionSW ||
                enableStabilizationSW != other.enableStabilizationSW ||
                authorizeSw != other.authorizeSw ||
                switchCameraSW != other.switchCameraSW ||
                menuUpSW != other.menuUpSW ||
                menuDownSW != other.menuDownSW ||
                menuValSw != other.menuValSw ||
                speedSW != other.speedSW ||
                fireMode != other.fireMode ||
                panelTemperature != other.panelTemperature);
    }
};

/**
 * @brief PLC42 data structure - UPDATED FOR HOME POSITION DETECTION
 * 
 * HARDWARE MAPPING (MDUINO 42+ Station Control):
 * Digital Inputs (8 total):
 *   DI0 (I0_0) → stationUpperSensor (Upper limit)
 *   DI1 (I0_1) → stationLowerSensor (Lower limit)
 *   DI2 (I0_2) → hatchState (Hatch position)
 *   DI3 (I0_3) → freeGimbalState (FREE toggle switch - LOCAL CONTROL)
 *   DI4 (I0_4) → ammunitionLevel (Ammo sensor)
 *   DI5 (I0_5) → Reserved (future E-STOP button)
 *   DI6 (I0_6) → azimuthHomeComplete  (Az HOME-END from Oriental Motor)
 *   DI7 (I0_7) → elevationHomeComplete   (El HOME-END from Oriental Motor)
 * 
 * Holding Registers (10 total):
 *   HR0: solenoidMode (1=Single, 2=Burst, 3=Continuous)
 *   HR1: gimbalOpMode (0=Manual, 1=Stop, 3=Home, 4=Free)
 *   HR2-3: azimuthSpeed (32-bit)
 *   HR4-5: elevationSpeed (32-bit)
 *   HR6: azimuthDirection
 *   HR7: elevationDirection
 *   HR8: solenoidState (0=OFF, 1=ON)
 *   HR9: resetAlarm (0=Normal, 1=Reset)
 *   HR10: azimuthReset (0=Normal, 1=Set Preset Home)
 */
struct Plc42Data {
    bool isConnected = false;

    // =========================================================================
    // DISCRETE INPUTS (8 inputs)
    // =========================================================================
    bool stationUpperSensor = false;       ///< DI0: Upper limit sensor
    bool stationLowerSensor = false;       ///< DI1: Lower limit sensor
    bool hatchState = false;               ///< DI2: Hatch state
    bool freeGimbalState = false;          ///< DI3: FREE mode toggle (LOCAL)
    bool ammunitionLevel = false;          ///< DI4: Ammunition level
    // DI5: Reserved (future E-STOP button)
    bool azimuthHomeComplete = false;      ///< DI6: Az HOME-END signal ⭐ NEW
    bool elevationHomeComplete = false;    ///< DI7: El HOME-END signal ⭐ NEW
    
    // Derived values (computed in parser)
    bool emergencyStopActive = false;      ///< Derived: gimbalOpMode == 1
    bool solenoidActive = false;           ///< Derived: solenoidState != 0

    // =========================================================================
    // HOLDING REGISTERS (11 registers)
    // =========================================================================
    uint16_t solenoidMode = 0;          ///< HR0: Fire mode
    uint16_t gimbalOpMode = 0;          ///< HR1: Gimbal operation mode
    uint32_t azimuthSpeed = 0;          ///< HR2-3: Azimuth speed (32-bit)
    uint32_t elevationSpeed = 0;        ///< HR4-5: Elevation speed (32-bit)
    uint16_t azimuthDirection = 0;      ///< HR6: Azimuth direction
    uint16_t elevationDirection = 0;    ///< HR7: Elevation direction
    uint16_t solenoidState = 0;         ///< HR8: Trigger command
    uint16_t resetAlarm = 0;            ///< HR9: Error reset
    uint16_t azimuthReset = 0;          ///< HR10: Azimuth reset (0=Normal, 1=Set Preset Home)

    bool operator!=(const Plc42Data &other) const {
        return (isConnected != other.isConnected ||
                stationUpperSensor != other.stationUpperSensor ||
                stationLowerSensor != other.stationLowerSensor ||
                hatchState != other.hatchState ||
                freeGimbalState != other.freeGimbalState ||
                ammunitionLevel != other.ammunitionLevel ||
                azimuthHomeComplete != other.azimuthHomeComplete ||     
                elevationHomeComplete != other.elevationHomeComplete || 
                emergencyStopActive != other.emergencyStopActive ||
                solenoidActive != other.solenoidActive ||
                solenoidMode != other.solenoidMode ||
                gimbalOpMode != other.gimbalOpMode ||
                azimuthSpeed != other.azimuthSpeed ||
                elevationSpeed != other.elevationSpeed ||
                azimuthDirection != other.azimuthDirection ||
                elevationDirection != other.elevationDirection ||
                solenoidState != other.solenoidState ||
                resetAlarm != other.resetAlarm ||
                azimuthReset != other.azimuthReset);
    }
};

// ============================================================================
// INPUT DEVICE DATA STRUCTURES
// ============================================================================

/**
 * @brief Joystick input device data structure (SDL2-based)
 */
struct JoystickData {
    bool isConnected = false;

    // Axis values (normalized -1.0 to 1.0)
    float axisX = 0.0f;
    float axisY = 0.0f;

    // Hat switch state (0 = centered, 1 = up, 2 = right, 4 = down, 8 = left)
    int hatState = 0;

    // Button states (up to 16 buttons)
    static const int MAX_BUTTONS = 16;
    bool buttons[MAX_BUTTONS] = { false };

    bool operator!=(const JoystickData &other) const {
        if (isConnected != other.isConnected ||
            !qFuzzyCompare(axisX + 1.0f, other.axisX + 1.0f) ||
            !qFuzzyCompare(axisY + 1.0f, other.axisY + 1.0f) ||
            hatState != other.hatState) {
            return true;
        }
        for (int i = 0; i < MAX_BUTTONS; i++) {
            if (buttons[i] != other.buttons[i]) {
                return true;
            }
        }
        return false;
    }
};

#endif // DATATYPES_H

#ifndef SYSTEMSTATEDATA_H
#define SYSTEMSTATEDATA_H

/**
 * @file SystemStateData.h
 * @brief Core data structures and enumerations for Remote Controlled Weapon System (RCWS)
 * 
 * This header defines all fundamental data structures, enumerations, and constants
 * used throughout the RCWS application for state management and system operation.
 * 
 * MAIN COMPONENTS:
 * 1. System Constants - Color definitions and default values
 * 2. Core Enumerations - System modes, states, and operational parameters
 * 3. Zone Management Structures - Area zones, sector scan zones, and target reference points
 * 4. System State Structure - Complete system state with organized data categories
 * 
 * DATA ORGANIZATION WITHIN SystemStateData:
 * • Operational State & Modes - Current and previous operational modes
 * • Display & UI Configuration - Visual settings and screen parameters
 * • Zone Management - All zone types and active zone tracking
 * • Camera Systems - Day/night camera status and control
 * • Gimbal & Positioning System - Physical positioning and motor status
 * • Orientation & Stabilization - Platform orientation and stabilization
 * • Laser Range Finder (LRF) - Distance measurement and status
 * • Joystick & Manual Controls - User input and manual control states
 * • Weapon System Control (PLC21) - Weapon arming and fire control
 * • Gimbal Station Hardware (PLC42) - Hardware sensors and monitoring
 * • Tracking System - Target tracking and movement control
 * • Ballistics & Fire Control - Zeroing, windage, and lead compensation
 * • Status & Information Display - System messages and status text
 * 
 * HELPER FUNCTIONS:
 * • System readiness checks
 * • Health monitoring utilities
 * • Comparison operators for state management
 *
 * @author MB
 * @date [Current Date]
 * @version 1.0
 */

#include <QString>
#include <QColor>
#include <QDateTime>
#include <QPointF>
#include <QtGlobal> // For qFuzzyCompare
#include <vector>
#include "utils/colorutils.h" // For ColorUtils
#include <vpi/algo/DCFTracker.h> // VPITrackingState, VPIDCFTrackedBoundingBox

// =================================
// CONSTANTS
// =================================

/**
 * @brief Color constants for tracking system status indication
 */
const QColor COLOR_TRACKING_ACQUIRING = Qt::yellow;      ///< Target acquisition in progress
const QColor COLOR_TRACKING_ACTIVE = QColor(70, 226, 165);  ///< Active tracking engaged
const QColor COLOR_TRACKING_COASTING = Qt::cyan;        ///< Coasting mode (temporary track loss)
const QColor COLOR_TRACKING_LOST = QColor(200,20,40);             ///< Target tracking lost
const QColor COLOR_TRACKING_DEFAULT = QColor(70, 226, 165); ///< Default tracking color
const QColor COLOR_TRACKING_FIRING = QColor(255, 255, 0); ///< Firing mode active (green)

// =================================
// ENUMERATIONS
// =================================

/**
 * @brief Available reticle types for weapon aiming system
 */
enum class ReticleType {
    BoxCrosshair,       ///< Box-style crosshair with center box (General purpose - NATO standard)
    BracketsReticle,    ///< Corner brackets style with crosshair (Enhanced visibility)
    DuplexCrosshair,    ///< Thick outer, thin inner crosshair (Sniper/precision style)
    FineCrosshair,      ///< Thin precision crosshair with range ticks (Long range)
    ChevronReticle,     ///< Downward pointing chevron with holdover marks (CQB style)
    COUNT               ///< Total number of reticle types (for iteration)
};

/**
 * @brief Weapon firing modes available in the system
 */
enum class FireMode { 
    SingleShot,  ///< Single round per trigger pull
    ShortBurst,  ///< Short controlled burst
    LongBurst,   ///< Extended burst fire
    Unknown      ///< Unknown or uninitialized fire mode
};

/**
 * @brief High-level operational modes of the weapon system
 */
enum class OperationalMode { 
    Idle,         ///< System idle, no active operations
    Surveillance, ///< Area surveillance mode
    Tracking,     ///< Target tracking mode
    Engagement ,   ///< Active engagement mode
    EmergencyStop , ///< Emergency stop mode
};

/**
 * @brief Motion control modes for gimbal and weapon positioning
 */
enum class MotionMode { 
    Manual,        ///< Manual joystick control
    Pattern,       ///< Predefined pattern scanning
    AutoTrack,     ///< Automatic target tracking
    ManualTrack,   ///< Manual target tracking
    RadarTracking, ///< Radar-assisted tracking
    Idle,          ///< No motion, idle state
    AutoSectorScan,///< Automatic sector scanning
    TRPScan,        ///< Target Reference Point scanning
    RadarSlew,
    MotionFree  ///< Gimbal free movement mode
};

/**
 * @brief Homing sequence state machine states
 * 
 * Tracks the complete homing operation from button press to completion.
 * The homing process sends a HOME command to Oriental Motor servo drives,
 * which move to their mechanical home position and send a HOME-END signal.
 */
enum class HomingState {
    Idle,        ///< No homing operation in progress
    Requested,   ///< Home button pressed, preparing to send command
    InProgress,  ///< HOME command sent to PLC42, waiting for HOME-END signal
    Completed,   ///< HOME-END received, homing successful
    Failed,      ///< Homing failed due to timeout
    Aborted      ///< Homing aborted by emergency stop or other condition
};

enum class TrackingPhase {
    Off,                  // Tracking is completely inactive.
    Acquisition,          // User is positioning/sizing the initial tracking gate.
    Tracking_LockPending, // System has a gate, attempting to lock (e.g., solid yellow box).
    Tracking_ActiveLock,  // System has a solid lock, gimbal is actively following (e.g., dashed red box).
    Tracking_Coast,       // Target is temporarily lost/occluded, system is predicting (e.g., dashed yellow box).
    Tracking_Firing       // Weapon has fired while locked, system holds position (e.g., dashed green box).
};

/**
 * @brief Zone classification types for operational areas
 */
enum class ZoneType {
    None,                  ///< No zone type assigned
    Safety,                ///< Safety zone (general restriction)
    NoTraverse,           ///< No-traverse zone (movement restricted)
    NoFire,               ///< No-fire zone (firing prohibited)
    AutoSectorScan,       ///< Automatic sector scan area
    TargetReferencePoint  ///< Target reference point zone
};

/**
 * @brief Lead angle compensation system status
 */
enum class LeadAngleStatus {
    Off,     ///< Lead angle compensation disabled
    On,      ///< Lead angle compensation active and functioning
    Lag,     ///< Lead angle calculation at maximum limit
    ZoomOut  ///< Lead angle too large for current FOV, zoom out required
};

/**
 * @brief Ammunition feed cycle FSM states
 * Tracks the state of the ammunition feeding mechanism
 */
enum class AmmoFeedState {
    Idle,       ///< Ready for new cycle - displays "IDLE"
    Extending,  ///< Actuator moving to extended position - displays "FEED..."
    Extended,   ///< Actuator at extended position, waiting for button release - displays "HOLD"
    Retracting, ///< Actuator returning to home - displays "FEED..."
    Fault       ///< Timeout/error occurred - displays "FAULT" (red, requires reset)
};

// =================================
// ZONE STRUCTURES
// =================================

/**
 * @brief Defines a 3D area zone with azimuth, elevation, and range constraints
 */
struct AreaZone {
    int id = -1;                        ///< Unique identifier for the zone
    ZoneType type = ZoneType::Safety;   ///< Type of zone (safety, no-fire, etc.)
    bool isEnabled = false;             ///< Whether the zone is currently active
    bool isFactorySet = false;          ///< Whether this is a factory-configured zone
    bool isOverridable = false;         ///< Whether the zone can be overridden by operator
    float startAzimuth = 0.0f;          ///< Starting azimuth angle in degrees
    float endAzimuth = 0.0f;            ///< Ending azimuth angle in degrees
    float minElevation = 0.0f;          ///< Minimum elevation angle in degrees
    float maxElevation = 0.0f;          ///< Maximum elevation angle in degrees
    float minRange = 0.0f;              ///< Minimum range in meters
    float maxRange = 0.0f;              ///< Maximum range in meters
    QString name = "";                  ///< Human-readable zone name

    AreaZone() = default;
    
    /**
     * @brief Equality comparison operator for AreaZone
     * @param other The other AreaZone to compare with
     * @return True if zones are identical, false otherwise
     */
    bool operator==(const AreaZone& other) const {
        return id == other.id && 
               type == other.type && 
               isEnabled == other.isEnabled &&
               isFactorySet == other.isFactorySet && 
               isOverridable == other.isOverridable &&
               qFuzzyCompare(startAzimuth, other.startAzimuth) && 
               qFuzzyCompare(endAzimuth, other.endAzimuth) &&
               qFuzzyCompare(minElevation, other.minElevation) && 
               qFuzzyCompare(maxElevation, other.maxElevation) &&
               qFuzzyCompare(minRange, other.minRange) && 
               qFuzzyCompare(maxRange, other.maxRange) &&
               name == other.name;
    }
    
    /**
     * @brief Inequality comparison operator for AreaZone
     * @param other The other AreaZone to compare with
     * @return True if zones are different, false otherwise
     */
    bool operator!=(const AreaZone& other) const { 
        return !(*this == other); 
    }
};

/**
 * @brief Defines an automatic sector scanning zone with two boundary points
 */
struct AutoSectorScanZone {
    int id = -1;                ///< Unique identifier for the scan zone
    bool isEnabled = false;     ///< Whether the scan zone is currently active
    float az1 = 0.0f;          ///< First boundary point azimuth in degrees
    float el1 = 0.0f;          ///< First boundary point elevation in degrees
    float az2 = 0.0f;          ///< Second boundary point azimuth in degrees
    float el2 = 0.0f;          ///< Second boundary point elevation in degrees
    float scanSpeed = 20.0f;   ///< Scanning speed in degrees per second

    AutoSectorScanZone() = default;
    
    /**
     * @brief Equality comparison operator for AutoSectorScanZone
     * @param other The other AutoSectorScanZone to compare with
     * @return True if scan zones are identical, false otherwise
     */
    bool operator==(const AutoSectorScanZone& other) const {
        return id == other.id && 
               isEnabled == other.isEnabled &&
               qFuzzyCompare(az1, other.az1) && 
               qFuzzyCompare(el1, other.el1) &&
               qFuzzyCompare(az2, other.az2) && 
               qFuzzyCompare(el2, other.el2) &&
               qFuzzyCompare(scanSpeed, other.scanSpeed);
    }
    
    /**
     * @brief Inequality comparison operator for AutoSectorScanZone
     * @param other The other AutoSectorScanZone to compare with
     * @return True if scan zones are different, false otherwise
     */
    bool operator!=(const AutoSectorScanZone& other) const { 
        return !(*this == other); 
    }
};

/**
 * @brief Defines a target reference point for navigation and scanning
 */
struct TargetReferencePoint {
    int id = -1;                ///< Unique identifier for the TRP
    int locationPage = 1;       ///< Location page number for organization
    int trpInPage = 1;         ///< TRP number within the page
    float azimuth = 0.0f;      ///< TRP azimuth position in degrees
    float elevation = 0.0f;    ///< TRP elevation position in degrees
    float haltTime = 0.0f;     ///< Halt time at TRP in seconds

    TargetReferencePoint() = default;
    
    /**
     * @brief Equality comparison operator for TargetReferencePoint
     * @param other The other TargetReferencePoint to compare with
     * @return True if TRPs are identical, false otherwise
     */
    bool operator==(const TargetReferencePoint& other) const {
        return id == other.id && 
               locationPage == other.locationPage && 
               trpInPage == other.trpInPage &&
               qFuzzyCompare(azimuth, other.azimuth) && 
               qFuzzyCompare(elevation, other.elevation) &&
               qFuzzyCompare(haltTime, other.haltTime);
    }
    
    /**
     * @brief Inequality comparison operator for TargetReferencePoint
     * @param other The other TargetReferencePoint to compare with
     * @return True if TRPs are different, false otherwise
     */
    bool operator!=(const TargetReferencePoint& other) const { 
        return !(*this == other); 
    }
};

struct SimpleRadarPlot {
    quint32 id;
    float azimuth;
    float range;
    float relativeCourse;
    float relativeSpeed;

    bool operator==(const SimpleRadarPlot &other) const {
        return id == other.id &&
               qFuzzyCompare(azimuth, other.azimuth) &&
               qFuzzyCompare(relativeSpeed, other.relativeSpeed) &&
               qFuzzyCompare(range, other.range) &&
               qFuzzyCompare(relativeCourse, other.relativeCourse);
    }
};

// =================================
// MAIN SYSTEM STATE STRUCTURE
// =================================

/**
 * @brief Comprehensive system state structure containing all RCWS operational data
 * 
 * This structure serves as the central data repository for the entire RCWS system,
 * organizing all operational parameters, sensor data, control states, and status
 * information into logical categories for efficient access and management.
 */
struct SystemStateData {
    
    // =================================
    // OPERATIONAL STATE & MODES
    // =================================
    OperationalMode opMode = OperationalMode::Idle;         ///< Current operational mode
    OperationalMode previousOpMode = OperationalMode::Idle; ///< Previous operational mode for state transitions
    MotionMode motionMode = MotionMode::Idle;               ///< Current motion control mode
    MotionMode previousMotionMode = MotionMode::Idle;       ///< Previous motion mode for state transitions
    
    // =================================
    // DISPLAY & UI CONFIGURATION
    // =================================
    ReticleType reticleType = ReticleType::BoxCrosshair;    ///< Current reticle style
    ColorStyle osdColorStyle = ColorStyle::Green;           ///< On-screen display color style
    QColor colorStyle = QColor(70, 226, 165);              ///< Current UI color theme
    int currentImageWidthPx = 1024;                         ///< Current image width in pixels
    int currentImageHeightPx = 768;                         ///< Current image height in pixels
    float reticleAimpointImageX_px = currentImageWidthPx / 2.0f;  ///< Reticle X position with ZEROING ONLY (gun boresight)
    float reticleAimpointImageY_px = currentImageHeightPx / 2.0f; ///< Reticle Y position with ZEROING ONLY (gun boresight)
    float ccipImpactImageX_px = currentImageWidthPx / 2.0f;      ///< CCIP pipper X position with ZEROING + LEAD (impact prediction)
    float ccipImpactImageY_px = currentImageHeightPx / 2.0f;     ///< CCIP pipper Y position with ZEROING + LEAD (impact prediction)

    // =================================
    // ZONE MANAGEMENT
    // =================================
    std::vector<AreaZone> areaZones;                        ///< Collection of all area zones
    std::vector<AutoSectorScanZone> sectorScanZones;       ///< Collection of all sector scan zones
    std::vector<TargetReferencePoint> targetReferencePoints; ///< Collection of all target reference points
    int activeAutoSectorScanZoneId = 1;                     ///< Currently active sector scan zone ID
    int activeTRPLocationPage = 1;                          ///< Currently active TRP location page
    QString currentScanName;                                ///< Name of current scanning operation
    QString currentTRPScanName;                             ///< Name of current TRP scan operation
    bool isReticleInNoFireZone = false;                     ///< Whether reticle is in a no-fire zone
    bool isReticleInNoTraverseZone = false;                 ///< Whether reticle is in a no-traverse zone
    
    // =================================
    // CAMERA SYSTEMS
    // =================================
    // Day Camera
    double dayZoomPosition = 0.0;       ///< Day camera zoom position (0-1 normalized)
    double dayCurrentHFOV = 9.0;        ///< Day camera current horizontal field of view in degrees
    double dayCurrentVFOV = 9.0;        ///< Day camera current vertical field of view in degrees (approx. same as H for day camera)
    bool dayCameraConnected = false;    ///< Day camera connection status
    bool dayCameraError = false;        ///< Day camera error status
    quint8 dayCameraStatus = 0;         ///< Day camera detailed status code
    bool dayAutofocusEnabled = true;    ///< Day camera autofocus enabled status
    quint16 dayFocusPosition = 0;       ///< Day camera focus position (12-bit max)

    // Night Camera
    double nightZoomPosition = 0.0;     ///< Night camera zoom position (0-1 normalized)
    double nightCurrentHFOV = 10.4;     ///< Night camera current horizontal field of view in degrees (FLIR TAU 2 wide: 10.4°×8°, narrow: 5.2°×4°)
    double nightCurrentVFOV = 8.0;      ///< Night camera current vertical field of view in degrees (NOT square sensor! 640×512)
    bool nightCameraConnected = false;  ///< Night camera connection status
    bool nightCameraError = false;      ///< Night camera error status (true if errorState != 0x00)
    quint8 nightCameraStatus = 0;       ///< Night camera detailed status code
    quint8 nightDigitalZoomLevel = 1;   ///< Night camera digital zoom level (FIXED TYPE: was bool, now quint8)
    bool nightFfcInProgress = false;    ///< Night camera FFC (Flat Field Correction) in progress status
    quint16 nightVideoMode = 0;         ///< Night camera video mode (e.g., 0=Normal, 1=Low Light, etc.)
    qint16 nightFpaTemperature = 0;     ///< Night camera FPA temperature in Celsius × 10

    // Camera Control
    bool activeCameraIsDay = false;     ///< True if day camera is active, false if night camera
    
    // =================================
    // GIMBAL & POSITIONING SYSTEM
    // =================================
    double mechanicalGimbalAz = 0.0;   ///< Mechanical gimbal azimuth position in degrees (without software offsets)
    double mechanicalGimbalEl = 0.0;   ///< Mechanical gimbal elevation position in degrees (without software offsets)
    double gimbalAz = 0.0;              ///< Current gimbal azimuth position in degrees
    double gimbalEl = 0.0;              ///< Current gimbal elevation position in degrees

    // =================================
    // AZIMUTH HOME OFFSET CALIBRATION
    // =================================
    // Runtime compensation for ABZO encoder drift in Oriental Motor AZD-KD
    // Offset is stored in encoder steps and applied during position conversion
    // See: Issue - Random Azimuth Home Drift on AZD-KD ABZO Encoder (~40,000 steps / 70-80°)
    double azHomeOffsetSteps = 0.0;     ///< Azimuth home offset in encoder steps (drift compensation)
    bool homeCalibrationModeActive = false; ///< True when home calibration procedure is active
    bool azHomeOffsetApplied = false;   ///< True if a valid home offset is currently being applied

    // Azimuth Servo (Enhanced)
    bool azServoConnected = false;      ///< Azimuth servo connection status
    float azMotorTemp = 0.0f;           ///< Azimuth motor temperature in Celsius
    float azDriverTemp = 0.0f;          ///< Azimuth driver temperature in Celsius
    float azRpm = 0.0f;                 ///< Azimuth servo RPM
    float azTorque = 0.0f;              ///< Azimuth servo torque percentage (0-100)
    bool azFault = false;               ///< Azimuth servo fault status

    // Elevation Servo (Enhanced)
    bool elServoConnected = false;      ///< Elevation servo connection status
    float elMotorTemp = 0.0f;           ///< Elevation motor temperature in Celsius
    float elDriverTemp = 0.0f;          ///< Elevation driver temperature in Celsius
    float elRpm = 0.0f;                 ///< Elevation servo RPM
    float elTorque = 0.0f;              ///< Elevation servo torque percentage (0-100)
    bool elFault = false;               ///< Elevation servo fault status

    float reticleAz = 0.0f;             ///< Reticle azimuth position in degrees
    float reticleEl = 0.0f;             ///< Reticle elevation position in degrees


    // =================================
    // SERVO ACTUATOR  
    // =================================
    bool actuatorConnected = false;      ///< Servo actuator connection status
    double actuatorPosition = 0.0;       ///< Linear actuator position in mm
    double actuatorVelocity = 0.0;       ///< Actuator velocity in mm/s
    double actuatorTemp = 0.0;           ///< Actuator temperature in Celsius
    double actuatorBusVoltage = 0.0;     ///< Actuator bus voltage in volts
    double actuatorTorque = 0.0;         ///< Actuator torque percentage (0-100)
    bool actuatorMotorOff = false;       ///< Actuator motor off status
    bool actuatorFault = false;          ///< Actuator fault/latching fault status
 

    
    // =================================
    // ORIENTATION & STABILIZATION
    // =================================
    bool imuConnected = false;          ///< IMU connection status
    double imuRollDeg = 0.0;            ///< IMU Roll angle in degrees
    double imuPitchDeg = 0.0;           ///< IMU Pitch angle in degrees
    double imuYawDeg = 0.0;             ///< IMU Yaw angle in degrees
    double imuTemp = 0.0;               ///< IMU temperature in Celsius
    double GyroX = 0.0;                 ///< Gyro X-axis rate in deg/s
    double GyroY = 0.0;                 ///< Gyro Y-axis rate in deg/s
    double GyroZ = 0.0;                 ///< Gyro Z-axis rate in deg/s
    double AccelX = 0.0;                ///< Accelerometer X-axis in G
    double AccelY = 0.0;                ///< Accelerometer Y-axis in G
    double AccelZ = 0.0;                ///< Accelerometer Z-axis in G
    bool isStabilizationActive = false; ///< Stabilization system active status
    double temperature = 0.0;           ///< Current system temperature in Celsius

    // World-frame stabilization target (AHRS-based absolute pointing)
    double targetAzimuth_world = 0.0;   ///< Desired world azimuth in degrees (0° = North, 90° = East)
    double targetElevation_world = 0.0; ///< Desired world elevation in degrees (0° = horizon, +90° = zenith)
    bool useWorldFrameTarget = false;   ///< Enable world-frame stabilization (true = hold absolute direction)

    // Stationary detection variables
    bool isVehicleStationary = false;   ///< Flag indicating if the vehicle is stationary
    double previousAccelMagnitude = 0.0; ///< Previous accelerometer magnitude for delta calculation
    QDateTime stationaryStartTime;      ///< Timestamp when stationary conditions began
    
    // =================================
    // LASER RANGE FINDER (LRF)
    // =================================
    bool lrfConnected = false;          ///< LRF connection status
    double lrfDistance = 900.0;         ///< Last measured distance in meters
    float lrfTemp = 0.0f;               ///< LRF temperature in Celsius
    quint32 lrfLaserCount = 0;          ///< Count of laser pulses emitted (FIXED TYPO)
    quint8 lrfSystemStatus = 0;         ///< LRF system status code
    bool lrfFault = false;              ///< LRF fault status
    bool lrfNoEcho = false;             ///< LRF no-echo received status
    bool lrfLaserNotOut = false;        ///< LRF laser not outputting status
    bool lrfOverTemp = false;           ///< LRF over-temperature condition
    quint8 isOverTemperature = 0;       ///< LRF over-temperature status (1 = true, 0 = false)

    // =================================
    // --- Radar Data ---
    // =================================
    QVector<SimpleRadarPlot> radarPlots;         // The latest list of all detected plots
    quint32 selectedRadarTrackId = 0;      // The ID of the target the user has selected from the list
    
    // =================================
    // JOYSTICK & MANUAL CONTROLS
    // =================================
    bool joystickConnected = false;     ///< Joystick connection status
    bool deadManSwitchActive = false;   ///< Safety dead man switch status
    float joystickAzValue = 0.0f;       ///< Joystick azimuth axis value (-1.0 to 1.0)
    float joystickElValue = 0.0f;       ///< Joystick elevation axis value (-1.0 to 1.0)
    bool upTrackButton = false;         ///< Up track button status
    bool downTrackButton = false;       ///< Down track button status
    bool menuUp = false;                  ///< Up switch status
    bool menuDown = false;                ///< Down switch status
    bool menuVal = false;             ///< Menu validation switch status
    int joystickHatDirection = 0;         ///< Joystick hat switch direction (0-7, 0 = center, 1 = up, 2 = up-right, etc.)

    
    // =================================
    // WEAPON SYSTEM CONTROL (PLC21)
    // =================================
    bool plc21Connected = false;          ///< PLC21 connection status
    bool stationEnabled = true;         ///< Weapon station enable status
    bool gotoHomePosition = false;                ///< Home position switch status
    bool gunArmed = false;              ///< Weapon arming status
    bool ammoLoadButtonPressed = false;            ///< Ammunition loaded status
    bool authorized = false;            ///< System authorization status
    bool detectionEnabled = false;      ///< Target detection enable status
    FireMode fireMode = FireMode::Unknown; ///< Current weapon fire mode
    double gimbalSpeed = 2.0;               ///< Speed switch setting
    bool enableStabilization = true;   ///< Platform stabilization enable switch
    bool emergencyStopActive = false;    ///< Emergency stop activation status

    // =================================
    // GIMBAL STATION HARDWARE (PLC42)
    // =================================
    // Limit Sensors
    bool plc42Connected = false;          ///< PLC42 connection status
    bool upperLimitSensorActive = false; ///< Upper travel limit sensor status
    bool lowerLimitSensorActive = false; ///< Lower travel limit sensor status
    
    // Station Inputs
    bool stationAmmunitionLevel = false; ///< Station ammunition level sensor
    bool hatchState = false;          ///< General station input 1
    bool freeGimbalState = false;          ///< General station input 2
    bool stationInput3 = false;          ///< General station input 3
     bool azimuthHomeComplete = false;      ///< Az HOME-END signal from Oriental Motor (DI6/I0_6)
    bool elevationHomeComplete = false;    ///< El HOME-END signal from Oriental Motor (DI7/I0_7)   
    // Environmental Monitoring
    int panelTemperature = 0;            ///< Control panel temperature in Celsius
    int stationTemperature = 0;          ///< Station ambient temperature in Celsius
    int stationPressure = 0;             ///< Station atmospheric pressure
    
    // Control States
    uint16_t solenoidMode = 0;           ///< Solenoid valve mode setting
    uint16_t gimbalOpMode = 0;           ///< Gimbal operational mode
    uint32_t azimuthSpeed = 0;           ///< Azimuth movement speed setting
    uint32_t elevationSpeed = 0;         ///< Elevation movement speed setting
    uint16_t azimuthDirection = 0;       ///< Azimuth movement direction
    uint16_t elevationDirection = 0;     ///< Elevation movement direction
    uint16_t solenoidState = 0;          ///< Current solenoid state
    uint16_t resetAlarm = 0;             ///< Alarm reset control
    uint16_t homePosition= 0;    ///< Home position command
    uint16_t stopGimbal = 0;      ///< Stop gimbal command

   // In "OPERATIONAL STATE & MODES" section, ADD:
    HomingState homingState = HomingState::Idle;  ///< Current homing sequence state
    // =================================
    // TRACKING SYSTEM
    // =================================
    bool upTrack = false;                ///< Up tracking button status
    bool downTrack = false;              ///< Down tracking button status
    bool valTrack = false;               ///< Track validation button status
    bool startTracking = false;          ///< Start tracking command status
    bool requestTrackingRestart = false; ///< Tracking restart request status
    bool trackingActive = false;         ///< Current tracking active status
    float trackingConfidence = 0.0f;     ///< Tracking confidence score (0.0-1.0) from VPI tracker
    double targetAz = 0.0;               ///< Current target azimuth in degrees
    double targetEl = 0.0;               ///< Current target elevation in degrees
    float trackedTargetVelocityX_px_s = 0.0;
    float trackedTargetVelocityY_px_s = 0.0;
    // --- Active Tracked Target Data (from Video Processor) ---

    float currentCameraHfovDegrees = 45.0f; // Set by CameraController

    bool  trackerHasValidTarget = false;
    float trackedTargetCenterX_px = 0.0f;
    float trackedTargetCenterY_px = 0.0f;
    float trackedTargetWidth_px = 0.0f;
    float trackedTargetHeight_px = 0.0f;
    VPITrackingState trackedTargetState = VPI_TRACKING_STATE_LOST; // Store the raw tracker state
    TrackingPhase currentTrackingPhase = TrackingPhase::Off;

    // The acquisition gate/box, defined by user before lock-on.
    // In IMAGE PIXEL coordinates.
    float acquisitionBoxX_px = 512.0f;
    float acquisitionBoxY_px = 384.0f;
    float acquisitionBoxW_px = 100.0f; // Default size
    float acquisitionBoxH_px = 100.0f;

    // =================================
    // BALLISTICS & FIRE CONTROL
    // =================================
    // Zeroing System
    bool zeroingModeActive = false;         ///< Weapon zeroing mode active status
    float zeroingAzimuthOffset = 0.0f;      ///< Zeroing azimuth offset in degrees
    float zeroingElevationOffset = 0.0f;    ///< Zeroing elevation offset in degrees
    bool zeroingAppliedToBallistics = false; ///< Whether zeroing is applied to ballistic calculations
    
    // Windage Compensation
    bool windageModeActive = false;         ///< Windage compensation mode active status
    float windageSpeedKnots = 0.0f;         ///< Wind speed for windage calculation in knots
    float windageDirectionDegrees = 0.0f;   ///< Wind direction for windage calculation in degrees
    bool windageAppliedToBallistics = false; ///< Whether windage is applied to ballistic calculations
    bool windageDirectionCaptured = false;
    float calculatedCrosswindMS = 0.0f;     ///< Calculated crosswind component in m/s (from windage + azimuth)

    // Environmental Conditions (for Ballistic LUT)
    // NOTE: Crosswind is NOT stored here - it's calculated from windage (direction + speed)
    //       and current gimbal azimuth. See WeaponController::calculateCrosswindComponent()
    bool environmentalModeActive = false;   ///< Environmental settings mode active status
    float environmentalTemperatureCelsius = 15.0f; ///< Air temperature in degrees Celsius (ISO standard: 15°C)
    float environmentalAltitudeMeters = 0.0f;      ///< Altitude above sea level in meters
    bool environmentalAppliedToBallistics = false; ///< Whether environmental settings are applied    
    // Lead Angle Compensation
    bool leadAngleCompensationActive = false;           ///< Lead angle compensation active status (NOW controls motion lead ONLY)
    LeadAngleStatus currentLeadAngleStatus = LeadAngleStatus::Off; ///< Current lead angle system status
    float leadAngleOffsetAz = 0.0f;                     ///< DEPRECATED - use motionLeadOffsetAz (kept for backward compat)
    float leadAngleOffsetEl = 0.0f;                     ///< DEPRECATED - use motionLeadOffsetEl (kept for backward compat)

    // Ballistic Drop Compensation (Professional FCS - Auto when LRF valid)
    float ballisticDropOffsetAz = 0.0f;                 ///< Auto ballistic drop azimuth offset (wind deflection) in degrees
    float ballisticDropOffsetEl = 0.0f;                 ///< Auto ballistic drop elevation offset (gravity compensation) in degrees
    bool ballisticDropActive = false;                   ///< Auto-applied when valid LRF range exists

    // Motion Lead Compensation (Only when LAC toggle active)
    float motionLeadOffsetAz = 0.0f;                    ///< Moving target lead azimuth offset in degrees
    float motionLeadOffsetEl = 0.0f;                    ///< Moving target lead elevation offset in degrees

    // Target Parameters for Ballistics
    float currentTargetRange = 2000.0f;                 ///< Current target range in meters
    float currentTargetAngularRateAz = 0.0f;            ///< Target angular rate in azimuth (degrees/second)
    float currentTargetAngularRateEl = 0.0f;            ///< Target angular rate in elevation (degrees/second)
    float muzzleVelocityMPS = 900.0f;                   ///< Projectile muzzle velocity in meters per second
    
    // Ammunition Feed Parameters
    AmmoFeedState ammoFeedState = AmmoFeedState::Idle;  ///< Current FSM state (for OSD display)
    bool ammoFeedCycleInProgress = false;   ///< FSM is running (for backward compat)
    bool ammoLoaded = false;                ///< Inferred from successful feed cycle completion
    // =================================
    // STATUS & INFORMATION DISPLAY
    // =================================
    QString weaponSystemStatus;         ///< Weapon system status message
    QString targetInformation;          ///< Current target information display
    QString gpsCoordinates;             ///< GPS coordinate information
    QString sensorReadings;             ///< Sensor readings summary
    QString alertsWarnings;             ///< System alerts and warnings
    QString leadStatusText = "";        ///< Lead angle status text for display
    QString zeroingStatusText = "";     ///< Zeroing status text for display
    
    // =================================
    // HELPER FUNCTIONS
    // =================================
    
    /**
     * @brief Checks if the weapon system is ready for operation
     * @return True if all safety and authorization conditions are met
     */
    bool isReady() const {
        return gunArmed && ammoLoaded && deadManSwitchActive && authorized;
    }
    
    /**
     * @brief Checks if the tracking system is ready for operation
     * @return True if tracking system is operational and safe
     */
    bool isTrackingReady() const {
        return trackingActive && !emergencyStopActive && stationEnabled;
    }
    
    /**
     * @brief Checks if the camera system is healthy and operational
     * @return True if the active camera is connected and error-free
     */
    bool isCameraSystemHealthy() const {
        return activeCameraIsDay ? (!dayCameraError && dayCameraConnected) 
                                 : (!nightCameraError && nightCameraConnected);
    }
    
    // =================================
    // COMPARISON OPERATORS
    // =================================
    
    /**
     * @brief Equality comparison operator for complete system state
     * @param other The other SystemStateData to compare with
     * @return True if all system state parameters are identical
     * 
     * NOTE: This operator now includes ALL member variables for complete state comparison
     */
    bool operator==(const SystemStateData& other) const {
        return 
               // Operational State & Modes
               opMode == other.opMode && 
               motionMode == other.motionMode && 
               previousOpMode == other.previousOpMode &&
               previousMotionMode == other.previousMotionMode &&
               
               // Display & UI Configuration
               reticleType == other.reticleType &&
               osdColorStyle == other.osdColorStyle &&
               colorStyle == other.colorStyle &&
               currentImageWidthPx == other.currentImageWidthPx &&
               currentImageHeightPx == other.currentImageHeightPx &&
               qFuzzyCompare(reticleAimpointImageX_px, other.reticleAimpointImageX_px) &&
               qFuzzyCompare(reticleAimpointImageY_px, other.reticleAimpointImageY_px) &&
               qFuzzyCompare(ccipImpactImageX_px, other.ccipImpactImageX_px) &&
               qFuzzyCompare(ccipImpactImageY_px, other.ccipImpactImageY_px) &&

               // Zone Management
               areaZones == other.areaZones &&
               sectorScanZones == other.sectorScanZones &&
               targetReferencePoints == other.targetReferencePoints &&
               activeAutoSectorScanZoneId == other.activeAutoSectorScanZoneId &&
               activeTRPLocationPage == other.activeTRPLocationPage &&
               currentScanName == other.currentScanName &&
               currentTRPScanName == other.currentTRPScanName &&
               isReticleInNoFireZone == other.isReticleInNoFireZone &&
               isReticleInNoTraverseZone == other.isReticleInNoTraverseZone &&
               
               // Camera Systems - Day Camera
               qFuzzyCompare(dayZoomPosition, other.dayZoomPosition) &&
               qFuzzyCompare(dayCurrentHFOV, other.dayCurrentHFOV) &&
               qFuzzyCompare(dayCurrentVFOV, other.dayCurrentVFOV) &&
               dayCameraConnected == other.dayCameraConnected &&
               dayCameraError == other.dayCameraError &&
               dayCameraStatus == other.dayCameraStatus &&
               dayAutofocusEnabled == other.dayAutofocusEnabled &&
               dayFocusPosition == other.dayFocusPosition &&

               // Camera Systems - Night Camera
               qFuzzyCompare(nightZoomPosition, other.nightZoomPosition) &&
               qFuzzyCompare(nightCurrentHFOV, other.nightCurrentHFOV) &&
               qFuzzyCompare(nightCurrentVFOV, other.nightCurrentVFOV) &&
               nightCameraConnected == other.nightCameraConnected &&
               nightCameraError == other.nightCameraError &&
               nightCameraStatus == other.nightCameraStatus &&
               nightDigitalZoomLevel == other.nightDigitalZoomLevel &&
               nightFfcInProgress == other.nightFfcInProgress &&
               nightVideoMode == other.nightVideoMode &&
               nightFpaTemperature == other.nightFpaTemperature &&

               // Camera Control
               activeCameraIsDay == other.activeCameraIsDay &&
               
               // Gimbal & Positioning System
               qFuzzyCompare(gimbalAz, other.gimbalAz) &&
               qFuzzyCompare(gimbalEl, other.gimbalEl) &&

               // Azimuth Home Offset Calibration
               qFuzzyCompare(azHomeOffsetSteps, other.azHomeOffsetSteps) &&
               homeCalibrationModeActive == other.homeCalibrationModeActive &&
               azHomeOffsetApplied == other.azHomeOffsetApplied &&

               // Azimuth Servo
               azServoConnected == other.azServoConnected &&
               qFuzzyCompare(azMotorTemp, other.azMotorTemp) &&
               qFuzzyCompare(azDriverTemp, other.azDriverTemp) &&
               qFuzzyCompare(azRpm, other.azRpm) &&
               qFuzzyCompare(azTorque, other.azTorque) &&
               azFault == other.azFault &&
               
               // Elevation Servo
               elServoConnected == other.elServoConnected &&
               qFuzzyCompare(elMotorTemp, other.elMotorTemp) &&
               qFuzzyCompare(elDriverTemp, other.elDriverTemp) &&
               qFuzzyCompare(elRpm, other.elRpm) &&
               qFuzzyCompare(elTorque, other.elTorque) &&
               elFault == other.elFault &&
               
               // Reticle Position
               qFuzzyCompare(reticleAz, other.reticleAz) &&
               qFuzzyCompare(reticleEl, other.reticleEl) &&
               
               // Servo Actuator
               actuatorConnected == other.actuatorConnected &&
               qFuzzyCompare(actuatorPosition, other.actuatorPosition) &&
               qFuzzyCompare(actuatorVelocity, other.actuatorVelocity) &&
               qFuzzyCompare(actuatorTemp, other.actuatorTemp) &&
               qFuzzyCompare(actuatorBusVoltage, other.actuatorBusVoltage) &&
               qFuzzyCompare(actuatorTorque, other.actuatorTorque) &&
               actuatorMotorOff == other.actuatorMotorOff &&
               actuatorFault == other.actuatorFault &&
               
               // Orientation & Stabilization
               imuConnected == other.imuConnected &&
               qFuzzyCompare(imuRollDeg, other.imuRollDeg) &&
               qFuzzyCompare(imuPitchDeg, other.imuPitchDeg) &&
               qFuzzyCompare(imuYawDeg, other.imuYawDeg) &&
               qFuzzyCompare(imuTemp, other.imuTemp) &&
               qFuzzyCompare(GyroX, other.GyroX) &&
               qFuzzyCompare(GyroY, other.GyroY) &&
               qFuzzyCompare(GyroZ, other.GyroZ) &&
               qFuzzyCompare(AccelX, other.AccelX) &&
               qFuzzyCompare(AccelY, other.AccelY) &&
               qFuzzyCompare(AccelZ, other.AccelZ) &&
               isStabilizationActive == other.isStabilizationActive &&
               qFuzzyCompare(temperature, other.temperature) &&

               // World-frame Stabilization Target
               qFuzzyCompare(targetAzimuth_world, other.targetAzimuth_world) &&
               qFuzzyCompare(targetElevation_world, other.targetElevation_world) &&
               useWorldFrameTarget == other.useWorldFrameTarget &&

               // Stationary Detection
               isVehicleStationary == other.isVehicleStationary &&
               qFuzzyCompare(previousAccelMagnitude, other.previousAccelMagnitude) &&
               stationaryStartTime == other.stationaryStartTime &&
               
               // Laser Range Finder (LRF)
               lrfConnected == other.lrfConnected &&
               qFuzzyCompare(lrfDistance, other.lrfDistance) &&
               qFuzzyCompare(lrfTemp, other.lrfTemp) &&
               lrfLaserCount == other.lrfLaserCount &&
               lrfSystemStatus == other.lrfSystemStatus &&
               lrfFault == other.lrfFault &&
               lrfNoEcho == other.lrfNoEcho &&
               lrfLaserNotOut == other.lrfLaserNotOut &&
               lrfOverTemp == other.lrfOverTemp &&
               isOverTemperature == other.isOverTemperature &&
               
               // Radar Data
               radarPlots == other.radarPlots &&
               selectedRadarTrackId == other.selectedRadarTrackId &&
               
               // Joystick & Manual Controls
               joystickConnected == other.joystickConnected &&
               deadManSwitchActive == other.deadManSwitchActive &&
               qFuzzyCompare(joystickAzValue, other.joystickAzValue) &&
               qFuzzyCompare(joystickElValue, other.joystickElValue) &&
               upTrackButton == other.upTrackButton &&
               downTrackButton == other.downTrackButton &&
               menuUp == other.menuUp &&
               menuDown == other.menuDown &&
               menuVal == other.menuVal &&
               joystickHatDirection == other.joystickHatDirection &&
               
               // Weapon System Control (PLC21)
               plc21Connected == other.plc21Connected &&
               stationEnabled == other.stationEnabled &&
               gotoHomePosition == other.gotoHomePosition &&
               gunArmed == other.gunArmed &&
               ammoLoadButtonPressed == other.ammoLoadButtonPressed &&
               authorized == other.authorized &&
               detectionEnabled == other.detectionEnabled &&
               fireMode == other.fireMode &&
               qFuzzyCompare(gimbalSpeed, other.gimbalSpeed) &&
               enableStabilization == other.enableStabilization &&
               
               // Gimbal Station Hardware (PLC42)
               plc42Connected == other.plc42Connected &&
               upperLimitSensorActive == other.upperLimitSensorActive &&
               lowerLimitSensorActive == other.lowerLimitSensorActive &&
               emergencyStopActive == other.emergencyStopActive &&
               stationAmmunitionLevel == other.stationAmmunitionLevel &&
               hatchState == other.hatchState &&
               freeGimbalState == other.freeGimbalState &&
               stationInput3 == other.stationInput3 &&
               panelTemperature == other.panelTemperature &&
               stationTemperature == other.stationTemperature &&
               stationPressure == other.stationPressure &&
               solenoidMode == other.solenoidMode &&
               gimbalOpMode == other.gimbalOpMode &&
               azimuthSpeed == other.azimuthSpeed &&
               elevationSpeed == other.elevationSpeed &&
               azimuthDirection == other.azimuthDirection &&
               elevationDirection == other.elevationDirection &&
               solenoidState == other.solenoidState &&
               resetAlarm == other.resetAlarm &&
              azimuthHomeComplete == other.azimuthHomeComplete &&
              elevationHomeComplete == other.elevationHomeComplete &&
               homingState == other.homingState &&
               // Tracking System
               upTrack == other.upTrack &&
               downTrack == other.downTrack &&
               valTrack == other.valTrack &&
               startTracking == other.startTracking &&
               requestTrackingRestart == other.requestTrackingRestart &&
               trackingActive == other.trackingActive &&
               qFuzzyCompare(trackingConfidence, other.trackingConfidence) &&
               qFuzzyCompare(targetAz, other.targetAz) &&
               qFuzzyCompare(targetEl, other.targetEl) &&
               qFuzzyCompare(trackedTargetVelocityX_px_s, other.trackedTargetVelocityX_px_s) &&
               qFuzzyCompare(trackedTargetVelocityY_px_s, other.trackedTargetVelocityY_px_s) &&
               qFuzzyCompare(currentCameraHfovDegrees, other.currentCameraHfovDegrees) &&
               trackerHasValidTarget == other.trackerHasValidTarget &&
               qFuzzyCompare(trackedTargetCenterX_px, other.trackedTargetCenterX_px) &&
               qFuzzyCompare(trackedTargetCenterY_px, other.trackedTargetCenterY_px) &&
               qFuzzyCompare(trackedTargetWidth_px, other.trackedTargetWidth_px) &&
               qFuzzyCompare(trackedTargetHeight_px, other.trackedTargetHeight_px) &&
               trackedTargetState == other.trackedTargetState &&
               currentTrackingPhase == other.currentTrackingPhase &&
               qFuzzyCompare(acquisitionBoxX_px, other.acquisitionBoxX_px) &&
               qFuzzyCompare(acquisitionBoxY_px, other.acquisitionBoxY_px) &&
               qFuzzyCompare(acquisitionBoxW_px, other.acquisitionBoxW_px) &&
               qFuzzyCompare(acquisitionBoxH_px, other.acquisitionBoxH_px) &&
               
               // Ballistics & Fire Control
               zeroingModeActive == other.zeroingModeActive &&
               qFuzzyCompare(zeroingAzimuthOffset, other.zeroingAzimuthOffset) &&
               qFuzzyCompare(zeroingElevationOffset, other.zeroingElevationOffset) &&
               zeroingAppliedToBallistics == other.zeroingAppliedToBallistics &&
               windageModeActive == other.windageModeActive &&
               qFuzzyCompare(windageSpeedKnots, other.windageSpeedKnots) &&
               qFuzzyCompare(windageDirectionDegrees, other.windageDirectionDegrees) &&
               windageAppliedToBallistics == other.windageAppliedToBallistics &&
               windageDirectionCaptured == other.windageDirectionCaptured &&
               qFuzzyCompare(calculatedCrosswindMS, other.calculatedCrosswindMS) &&
               environmentalModeActive == other.environmentalModeActive &&
               qFuzzyCompare(environmentalTemperatureCelsius, other.environmentalTemperatureCelsius) &&
               qFuzzyCompare(environmentalAltitudeMeters, other.environmentalAltitudeMeters) &&
               environmentalAppliedToBallistics == other.environmentalAppliedToBallistics &&
               leadAngleCompensationActive == other.leadAngleCompensationActive &&
               currentLeadAngleStatus == other.currentLeadAngleStatus &&
               qFuzzyCompare(leadAngleOffsetAz, other.leadAngleOffsetAz) &&
               qFuzzyCompare(leadAngleOffsetEl, other.leadAngleOffsetEl) &&
               qFuzzyCompare(ballisticDropOffsetAz, other.ballisticDropOffsetAz) &&
               qFuzzyCompare(ballisticDropOffsetEl, other.ballisticDropOffsetEl) &&
               ballisticDropActive == other.ballisticDropActive &&
               qFuzzyCompare(motionLeadOffsetAz, other.motionLeadOffsetAz) &&
               qFuzzyCompare(motionLeadOffsetEl, other.motionLeadOffsetEl) &&
               qFuzzyCompare(currentTargetRange, other.currentTargetRange) &&
               qFuzzyCompare(currentTargetAngularRateAz, other.currentTargetAngularRateAz) &&
               qFuzzyCompare(currentTargetAngularRateEl, other.currentTargetAngularRateEl) &&
               qFuzzyCompare(muzzleVelocityMPS, other.muzzleVelocityMPS) &&
               ammoFeedState == other.ammoFeedState &&
               ammoFeedCycleInProgress == other.ammoFeedCycleInProgress &&
               ammoLoaded == other.ammoLoaded &&

               // Status & Information Display
               weaponSystemStatus == other.weaponSystemStatus &&
               targetInformation == other.targetInformation &&
               gpsCoordinates == other.gpsCoordinates &&
               sensorReadings == other.sensorReadings &&
               alertsWarnings == other.alertsWarnings &&
               leadStatusText == other.leadStatusText &&
               zeroingStatusText == other.zeroingStatusText;
    }
    
    bool operator!=(const SystemStateData& other) const {
        return !(*this == other);
    }
};

#endif // SYSTEMSTATEDATA_H
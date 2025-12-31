#ifndef STATEPARTITIONS_H
#define STATEPARTITIONS_H

/**
 * @file statepartitions.h
 * @brief Domain-specific state partition structs for SystemStateModel
 *
 * This header defines typed views over SystemStateData fields, organized by
 * domain responsibility. These partitions enable:
 * - Type-safe access to related state fields
 * - Clear ownership boundaries between subsystems
 * - Write protection for safety-critical state
 * - Future migration path toward proper domain separation
 *
 * DESIGN PRINCIPLES:
 * - These are views/wrappers, NOT copies of data
 * - Backward compatibility: existing code continues to work
 * - No logic moves: controllers still use SystemStateData directly
 * - Write protection: SafetyState can only be modified by authorized classes
 *
 * OWNERSHIP RULES:
 * - SafetyState: WRITE-RESTRICTED - only SafetyInterlock may modify
 * - WeaponState: WeaponController is primary owner
 * - GimbalState: GimbalController is primary owner
 * - TrackingState: TrackingController is primary owner
 *
 * USAGE:
 * @code
 * // Read-only access via typed accessor
 * if (model.safetyState().emergencyStopActive) {
 *     // Handle E-stop
 * }
 *
 * // Write access still goes through existing methods
 * // (SafetyState modification requires SafetyInterlock)
 * @endcode
 *
 * @date 2025-12-31
 * @version 1.0
 */

#include "systemstatedata.h"

// Forward declarations
class SystemStateModel;
class SafetyInterlock;
class EmergencyStopMonitor;

// =============================================================================
// SAFETY STATE PARTITION
// =============================================================================

/**
 * @brief Safety-critical state fields (WRITE-RESTRICTED)
 *
 * Contains all fields related to system safety and authorization.
 * Only SafetyInterlock (and EmergencyStopMonitor) may modify these fields.
 *
 * SAFETY HIERARCHY (per MIL-STD / CROWS):
 * 1. Emergency Stop - HIGHEST PRIORITY
 * 2. Hardware interlocks (limit switches)
 * 3. Software safety zones
 * 4. Operator authorization
 *
 * @warning Direct modification of these fields bypassing SafetyInterlock
 *          may result in safety violations. All writes should go through
 *          SafetyInterlock::checkFirePermission() or similar methods.
 */
struct SafetyState {
    // =========================================================================
    // EMERGENCY STOP
    // =========================================================================
    bool emergencyStopActive = false;     ///< E-stop button pressed (PLC42)

    // =========================================================================
    // STATION AUTHORIZATION
    // =========================================================================
    bool stationEnabled = false;          ///< Station master enable switch
    bool authorized = false;              ///< System authorized for operation

    // =========================================================================
    // WEAPON SAFETY
    // =========================================================================
    bool gunArmed = false;                ///< Weapon arming switch state
    bool deadManSwitchActive = false;     ///< Dead man switch pressed

    // =========================================================================
    // ZONE RESTRICTIONS
    // =========================================================================
    bool isReticleInNoFireZone = false;   ///< Aiming into NFZ
    bool isReticleInNoTraverseZone = false; ///< Aiming into NTZ

    // =========================================================================
    // HARDWARE INTERLOCKS
    // =========================================================================
    bool upperLimitSensorActive = false;  ///< Upper travel limit reached
    bool lowerLimitSensorActive = false;  ///< Lower travel limit reached

    // =========================================================================
    // DERIVED SAFETY CHECKS
    // =========================================================================

    /**
     * @brief Check if firing is blocked by any safety condition
     * @return true if ANY safety condition blocks firing
     */
    bool isFireBlocked() const {
        return emergencyStopActive ||
               !stationEnabled ||
               !authorized ||
               !gunArmed ||
               !deadManSwitchActive ||
               isReticleInNoFireZone;
    }

    /**
     * @brief Check if movement is blocked by any safety condition
     * @return true if ANY safety condition blocks movement
     */
    bool isMovementBlocked() const {
        return emergencyStopActive ||
               !stationEnabled ||
               isReticleInNoTraverseZone;
    }

    /**
     * @brief Get human-readable blocking reason for fire
     * @return QString describing why firing is blocked (empty if not blocked)
     */
    QString fireBlockReason() const {
        if (emergencyStopActive) return "EMERGENCY STOP";
        if (!stationEnabled) return "STATION DISABLED";
        if (!authorized) return "NOT AUTHORIZED";
        if (!gunArmed) return "GUN NOT ARMED";
        if (!deadManSwitchActive) return "DEAD MAN SWITCH";
        if (isReticleInNoFireZone) return "NO-FIRE ZONE";
        return QString();
    }
};

// =============================================================================
// WEAPON STATE PARTITION
// =============================================================================

/**
 * @brief Weapon system state fields
 *
 * Contains all fields related to weapon control, charging, and fire modes.
 * WeaponController is the primary owner of these fields.
 */
struct WeaponState {
    // =========================================================================
    // CHARGING SYSTEM (Cocking Actuator)
    // =========================================================================
    ChargingState chargingState = ChargingState::Idle; ///< Current FSM state
    bool chargeCycleInProgress = false;   ///< Charging operation running
    bool weaponCharged = false;           ///< Round chambered
    int chargeCyclesCompleted = 0;        ///< Completed cycles (for M2HB)
    int chargeCyclesRequired = 2;         ///< Required cycles for weapon type
    bool chargeLockoutActive = false;     ///< 4-second post-charge lockout

    // =========================================================================
    // WEAPON CONFIGURATION
    // =========================================================================
    WeaponType installedWeaponType = WeaponType::M2HB; ///< Current weapon
    FireMode fireMode = FireMode::Unknown; ///< Selected fire mode

    // =========================================================================
    // OPERATOR INPUT
    // =========================================================================
    bool chargeButtonPressed = false;     ///< CHG button state

    // =========================================================================
    // DERIVED CHECKS
    // =========================================================================

    /**
     * @brief Check if weapon is ready to fire
     * @return true if weapon is charged and not in lockout
     */
    bool isReadyToFire() const {
        return weaponCharged &&
               !chargeCycleInProgress &&
               !chargeLockoutActive &&
               chargingState == ChargingState::Idle;
    }

    /**
     * @brief Check if charging can be initiated
     * @return true if charging is allowed
     */
    bool canInitiateCharge() const {
        return chargingState == ChargingState::Idle &&
               !chargeLockoutActive;
    }
};

// =============================================================================
// GIMBAL STATE PARTITION
// =============================================================================

/**
 * @brief Gimbal positioning and servo state fields
 *
 * Contains all fields related to gimbal position, servo status, and
 * motion control. GimbalController is the primary owner of these fields.
 */
struct GimbalState {
    // =========================================================================
    // POSITION
    // =========================================================================
    double gimbalAz = 0.0;                ///< Current azimuth (with offsets)
    double gimbalEl = 0.0;                ///< Current elevation (with offsets)
    double mechanicalGimbalAz = 0.0;      ///< Mechanical azimuth (raw)
    double mechanicalGimbalEl = 0.0;      ///< Mechanical elevation (raw)

    // =========================================================================
    // MOTION CONTROL
    // =========================================================================
    MotionMode motionMode = MotionMode::Idle; ///< Current motion mode
    HomingState homingState = HomingState::Idle; ///< Homing FSM state
    bool gotoHomePosition = false;        ///< Home command requested

    // =========================================================================
    // AZIMUTH SERVO
    // =========================================================================
    bool azServoConnected = false;        ///< Azimuth servo online
    float azMotorTemp = 0.0f;             ///< Motor temperature (°C)
    float azDriverTemp = 0.0f;            ///< Driver temperature (°C)
    float azRpm = 0.0f;                   ///< Current RPM
    float azTorque = 0.0f;                ///< Torque percentage
    bool azFault = false;                 ///< Servo fault flag

    // =========================================================================
    // ELEVATION SERVO
    // =========================================================================
    bool elServoConnected = false;        ///< Elevation servo online
    float elMotorTemp = 0.0f;             ///< Motor temperature (°C)
    float elDriverTemp = 0.0f;            ///< Driver temperature (°C)
    float elRpm = 0.0f;                   ///< Current RPM
    float elTorque = 0.0f;                ///< Torque percentage
    bool elFault = false;                 ///< Servo fault flag

    // =========================================================================
    // ACTUATOR (Charging Mechanism)
    // =========================================================================
    bool actuatorConnected = false;       ///< Actuator online
    double actuatorPosition = 0.0;        ///< Position in mm
    double actuatorVelocity = 0.0;        ///< Velocity in mm/s
    bool actuatorFault = false;           ///< Actuator fault flag

    // =========================================================================
    // DERIVED CHECKS
    // =========================================================================

    /**
     * @brief Check if gimbal is operational
     * @return true if both servos are connected and not faulted
     */
    bool isOperational() const {
        return azServoConnected && elServoConnected &&
               !azFault && !elFault;
    }

    /**
     * @brief Check if homing is complete
     * @return true if homing succeeded
     */
    bool isHomed() const {
        return homingState == HomingState::Completed;
    }

    /**
     * @brief Check if any servo has a thermal warning
     * @param threshold Temperature threshold in Celsius (default 70°C)
     * @return true if any servo exceeds threshold
     */
    bool hasThermalWarning(float threshold = 70.0f) const {
        return azMotorTemp > threshold || azDriverTemp > threshold ||
               elMotorTemp > threshold || elDriverTemp > threshold;
    }
};

// =============================================================================
// TRACKING STATE PARTITION
// =============================================================================

/**
 * @brief Tracking system state fields
 *
 * Contains all fields related to target tracking, acquisition, and
 * tracker feedback. TrackingController is the primary owner of these fields.
 */
struct TrackingState {
    // =========================================================================
    // TRACKING STATUS
    // =========================================================================
    bool trackingActive = false;          ///< Tracking system engaged
    TrackingPhase currentTrackingPhase = TrackingPhase::Off; ///< Current phase
    float trackingConfidence = 0.0f;      ///< Tracker confidence (0.0-1.0)
    VPITrackingState trackedTargetState = VPI_TRACKING_STATE_LOST; ///< VPI state

    // =========================================================================
    // TRACKED TARGET
    // =========================================================================
    bool trackerHasValidTarget = false;   ///< Tracker has lock
    float trackedTargetCenterX_px = 0.0f; ///< Target center X (pixels)
    float trackedTargetCenterY_px = 0.0f; ///< Target center Y (pixels)
    float trackedTargetWidth_px = 0.0f;   ///< Target width (pixels)
    float trackedTargetHeight_px = 0.0f;  ///< Target height (pixels)
    float trackedTargetVelocityX_px_s = 0.0f; ///< Target X velocity (px/s)
    float trackedTargetVelocityY_px_s = 0.0f; ///< Target Y velocity (px/s)

    // =========================================================================
    // ACQUISITION GATE
    // =========================================================================
    float acquisitionBoxX_px = 512.0f;    ///< Gate center X (pixels)
    float acquisitionBoxY_px = 384.0f;    ///< Gate center Y (pixels)
    float acquisitionBoxW_px = 100.0f;    ///< Gate width (pixels)
    float acquisitionBoxH_px = 100.0f;    ///< Gate height (pixels)

    // =========================================================================
    // TARGET ANGULAR POSITION
    // =========================================================================
    double targetAz = 0.0;                ///< Target azimuth (degrees)
    double targetEl = 0.0;                ///< Target elevation (degrees)

    // =========================================================================
    // DERIVED CHECKS
    // =========================================================================

    /**
     * @brief Check if tracker has active lock
     * @return true if tracking with valid target
     */
    bool hasLock() const {
        return trackingActive &&
               trackerHasValidTarget &&
               (currentTrackingPhase == TrackingPhase::Tracking_ActiveLock ||
                currentTrackingPhase == TrackingPhase::Tracking_Firing);
    }

    /**
     * @brief Check if tracker is coasting (temporary loss)
     * @return true if in coast mode
     */
    bool isCoasting() const {
        return currentTrackingPhase == TrackingPhase::Tracking_Coast;
    }

    /**
     * @brief Check if acquisition is in progress
     * @return true if user is positioning acquisition gate
     */
    bool isAcquiring() const {
        return currentTrackingPhase == TrackingPhase::Acquisition ||
               currentTrackingPhase == TrackingPhase::Tracking_LockPending;
    }

    /**
     * @brief Get tracking quality indicator
     * @return "GOOD" if confidence > 0.7, "MARGINAL" if > 0.4, else "POOR"
     */
    QString qualityIndicator() const {
        if (trackingConfidence > 0.7f) return "GOOD";
        if (trackingConfidence > 0.4f) return "MARGINAL";
        return "POOR";
    }
};

// =============================================================================
// UTILITY: POPULATE PARTITION FROM SYSTEM STATE DATA
// =============================================================================

/**
 * @brief Populate SafetyState from SystemStateData
 * @param data Source system state data
 * @return Populated SafetyState struct
 */
inline SafetyState extractSafetyState(const SystemStateData& data) {
    SafetyState state;
    state.emergencyStopActive = data.emergencyStopActive;
    state.stationEnabled = data.stationEnabled;
    state.authorized = data.authorized;
    state.gunArmed = data.gunArmed;
    state.deadManSwitchActive = data.deadManSwitchActive;
    state.isReticleInNoFireZone = data.isReticleInNoFireZone;
    state.isReticleInNoTraverseZone = data.isReticleInNoTraverseZone;
    state.upperLimitSensorActive = data.upperLimitSensorActive;
    state.lowerLimitSensorActive = data.lowerLimitSensorActive;
    return state;
}

/**
 * @brief Populate WeaponState from SystemStateData
 * @param data Source system state data
 * @return Populated WeaponState struct
 */
inline WeaponState extractWeaponState(const SystemStateData& data) {
    WeaponState state;
    state.chargingState = data.chargingState;
    state.chargeCycleInProgress = data.chargeCycleInProgress;
    state.weaponCharged = data.weaponCharged;
    state.chargeCyclesCompleted = data.chargeCyclesCompleted;
    state.chargeCyclesRequired = data.chargeCyclesRequired;
    state.chargeLockoutActive = data.chargeLockoutActive;
    state.installedWeaponType = data.installedWeaponType;
    state.fireMode = data.fireMode;
    state.chargeButtonPressed = data.chargeButtonPressed;
    return state;
}

/**
 * @brief Populate GimbalState from SystemStateData
 * @param data Source system state data
 * @return Populated GimbalState struct
 */
inline GimbalState extractGimbalState(const SystemStateData& data) {
    GimbalState state;
    state.gimbalAz = data.gimbalAz;
    state.gimbalEl = data.gimbalEl;
    state.mechanicalGimbalAz = data.mechanicalGimbalAz;
    state.mechanicalGimbalEl = data.mechanicalGimbalEl;
    state.motionMode = data.motionMode;
    state.homingState = data.homingState;
    state.gotoHomePosition = data.gotoHomePosition;
    state.azServoConnected = data.azServoConnected;
    state.azMotorTemp = data.azMotorTemp;
    state.azDriverTemp = data.azDriverTemp;
    state.azRpm = data.azRpm;
    state.azTorque = data.azTorque;
    state.azFault = data.azFault;
    state.elServoConnected = data.elServoConnected;
    state.elMotorTemp = data.elMotorTemp;
    state.elDriverTemp = data.elDriverTemp;
    state.elRpm = data.elRpm;
    state.elTorque = data.elTorque;
    state.elFault = data.elFault;
    state.actuatorConnected = data.actuatorConnected;
    state.actuatorPosition = data.actuatorPosition;
    state.actuatorVelocity = data.actuatorVelocity;
    state.actuatorFault = data.actuatorFault;
    return state;
}

/**
 * @brief Populate TrackingState from SystemStateData
 * @param data Source system state data
 * @return Populated TrackingState struct
 */
inline TrackingState extractTrackingState(const SystemStateData& data) {
    TrackingState state;
    state.trackingActive = data.trackingActive;
    state.currentTrackingPhase = data.currentTrackingPhase;
    state.trackingConfidence = data.trackingConfidence;
    state.trackedTargetState = data.trackedTargetState;
    state.trackerHasValidTarget = data.trackerHasValidTarget;
    state.trackedTargetCenterX_px = data.trackedTargetCenterX_px;
    state.trackedTargetCenterY_px = data.trackedTargetCenterY_px;
    state.trackedTargetWidth_px = data.trackedTargetWidth_px;
    state.trackedTargetHeight_px = data.trackedTargetHeight_px;
    state.trackedTargetVelocityX_px_s = data.trackedTargetVelocityX_px_s;
    state.trackedTargetVelocityY_px_s = data.trackedTargetVelocityY_px_s;
    state.acquisitionBoxX_px = data.acquisitionBoxX_px;
    state.acquisitionBoxY_px = data.acquisitionBoxY_px;
    state.acquisitionBoxW_px = data.acquisitionBoxW_px;
    state.acquisitionBoxH_px = data.acquisitionBoxH_px;
    state.targetAz = data.targetAz;
    state.targetEl = data.targetEl;
    return state;
}

// =============================================================================
// ZONE STATE PARTITION
// =============================================================================

/**
 * @brief Zone management state fields
 *
 * Contains all fields related to area zones, sector scans, and TRPs.
 * ZoneEnforcementService and SystemStateModel are the primary owners.
 */
struct ZoneState {
    // =========================================================================
    // ACTIVE ZONE STATUS
    // =========================================================================
    bool isReticleInNoFireZone = false;   ///< Currently aiming into NFZ
    bool isReticleInNoTraverseZone = false; ///< Currently aiming into NTZ

    // =========================================================================
    // ZONE CONFIGURATION
    // =========================================================================
    int areaZoneCount = 0;                ///< Number of configured area zones
    int sectorScanZoneCount = 0;          ///< Number of sector scan zones
    int trpCount = 0;                     ///< Number of target reference points

    // =========================================================================
    // ACTIVE SELECTIONS
    // =========================================================================
    int activeAutoSectorScanZoneId = 1;   ///< Selected sector scan zone
    int activeTRPLocationPage = 1;        ///< Selected TRP location page
    QString currentScanName;              ///< Name of current scan operation
    QString currentTRPScanName;           ///< Name of current TRP scan

    // =========================================================================
    // DERIVED CHECKS
    // =========================================================================

    /**
     * @brief Check if any zone restriction is active
     * @return true if reticle is in any restricted zone
     */
    bool hasActiveRestriction() const {
        return isReticleInNoFireZone || isReticleInNoTraverseZone;
    }

    /**
     * @brief Check if zones are configured
     * @return true if at least one zone exists
     */
    bool hasZonesConfigured() const {
        return areaZoneCount > 0;
    }
};

// =============================================================================
// ENVIRONMENTAL STATE PARTITION
// =============================================================================

/**
 * @brief Environmental and ballistic compensation state fields
 *
 * Contains all fields related to ballistic corrections including zeroing,
 * windage, environmental conditions, and lead angle compensation.
 * WeaponController is the primary owner of these fields.
 */
struct EnvironmentalState {
    // =========================================================================
    // ZEROING COMPENSATION
    // =========================================================================
    bool zeroingModeActive = false;       ///< Zeroing procedure active
    float zeroingAzimuthOffset = 0.0f;    ///< Zeroing Az offset (degrees)
    float zeroingElevationOffset = 0.0f;  ///< Zeroing El offset (degrees)
    bool zeroingAppliedToBallistics = false; ///< Zeroing applied to CCIP

    // =========================================================================
    // WINDAGE COMPENSATION
    // =========================================================================
    bool windageModeActive = false;       ///< Windage procedure active
    float windageSpeedKnots = 0.0f;       ///< Wind speed (knots)
    float windageDirectionDegrees = 0.0f; ///< Wind direction (degrees)
    bool windageDirectionCaptured = false; ///< Direction captured
    bool windageAppliedToBallistics = false; ///< Windage applied to CCIP
    float calculatedCrosswindMS = 0.0f;   ///< Calculated crosswind (m/s)

    // =========================================================================
    // ENVIRONMENTAL CONDITIONS
    // =========================================================================
    bool environmentalModeActive = false; ///< Environmental procedure active
    float environmentalTemperatureCelsius = 15.0f; ///< Air temp (°C, ISA=15)
    float environmentalAltitudeMeters = 0.0f; ///< Altitude (m ASL)
    bool environmentalAppliedToBallistics = false; ///< Env applied to CCIP

    // =========================================================================
    // BALLISTIC DROP
    // =========================================================================
    bool ballisticDropActive = false;     ///< Auto ballistic drop active
    float ballisticDropOffsetAz = 0.0f;   ///< Wind deflection (degrees)
    float ballisticDropOffsetEl = 0.0f;   ///< Gravity drop (degrees)

    // =========================================================================
    // LEAD ANGLE COMPENSATION (LAC)
    // =========================================================================
    bool leadAngleCompensationActive = false; ///< LAC toggle active
    LeadAngleStatus currentLeadAngleStatus = LeadAngleStatus::Off; ///< LAC status
    float motionLeadOffsetAz = 0.0f;      ///< Motion lead Az (degrees)
    float motionLeadOffsetEl = 0.0f;      ///< Motion lead El (degrees)

    // =========================================================================
    // CROWS-COMPLIANT LAC LATCHING
    // =========================================================================
    bool lacArmed = false;                ///< LAC manually armed
    float lacLatchedAzRate_dps = 0.0f;    ///< Latched Az rate (deg/s)
    float lacLatchedElRate_dps = 0.0f;    ///< Latched El rate (deg/s)

    // =========================================================================
    // TARGET PARAMETERS
    // =========================================================================
    float currentTargetRange = 2000.0f;   ///< Target range (meters)
    float muzzleVelocityMPS = 900.0f;     ///< Muzzle velocity (m/s)

    // =========================================================================
    // DERIVED CHECKS
    // =========================================================================

    /**
     * @brief Check if any ballistic compensation is active
     * @return true if any compensation is being applied
     */
    bool hasActiveCompensation() const {
        return zeroingAppliedToBallistics ||
               windageAppliedToBallistics ||
               environmentalAppliedToBallistics ||
               ballisticDropActive ||
               leadAngleCompensationActive;
    }

    /**
     * @brief Check if zeroing is complete and applied
     * @return true if zeroing offsets are active
     */
    bool isZeroed() const {
        return zeroingAppliedToBallistics &&
               (zeroingAzimuthOffset != 0.0f || zeroingElevationOffset != 0.0f);
    }

    /**
     * @brief Check if LAC can be engaged (armed and ready)
     * @return true if LAC is armed and can be engaged
     */
    bool canEngageLAC() const {
        return lacArmed && !leadAngleCompensationActive;
    }
};

// =============================================================================
// UTILITY: POPULATE ZONE AND ENVIRONMENTAL PARTITIONS
// =============================================================================

/**
 * @brief Populate ZoneState from SystemStateData
 * @param data Source system state data
 * @return Populated ZoneState struct
 */
inline ZoneState extractZoneState(const SystemStateData& data) {
    ZoneState state;
    state.isReticleInNoFireZone = data.isReticleInNoFireZone;
    state.isReticleInNoTraverseZone = data.isReticleInNoTraverseZone;
    state.areaZoneCount = static_cast<int>(data.areaZones.size());
    state.sectorScanZoneCount = static_cast<int>(data.sectorScanZones.size());
    state.trpCount = static_cast<int>(data.targetReferencePoints.size());
    state.activeAutoSectorScanZoneId = data.activeAutoSectorScanZoneId;
    state.activeTRPLocationPage = data.activeTRPLocationPage;
    state.currentScanName = data.currentScanName;
    state.currentTRPScanName = data.currentTRPScanName;
    return state;
}

/**
 * @brief Populate EnvironmentalState from SystemStateData
 * @param data Source system state data
 * @return Populated EnvironmentalState struct
 */
inline EnvironmentalState extractEnvironmentalState(const SystemStateData& data) {
    EnvironmentalState state;
    // Zeroing
    state.zeroingModeActive = data.zeroingModeActive;
    state.zeroingAzimuthOffset = data.zeroingAzimuthOffset;
    state.zeroingElevationOffset = data.zeroingElevationOffset;
    state.zeroingAppliedToBallistics = data.zeroingAppliedToBallistics;
    // Windage
    state.windageModeActive = data.windageModeActive;
    state.windageSpeedKnots = data.windageSpeedKnots;
    state.windageDirectionDegrees = data.windageDirectionDegrees;
    state.windageDirectionCaptured = data.windageDirectionCaptured;
    state.windageAppliedToBallistics = data.windageAppliedToBallistics;
    state.calculatedCrosswindMS = data.calculatedCrosswindMS;
    // Environmental
    state.environmentalModeActive = data.environmentalModeActive;
    state.environmentalTemperatureCelsius = data.environmentalTemperatureCelsius;
    state.environmentalAltitudeMeters = data.environmentalAltitudeMeters;
    state.environmentalAppliedToBallistics = data.environmentalAppliedToBallistics;
    // Ballistic drop
    state.ballisticDropActive = data.ballisticDropActive;
    state.ballisticDropOffsetAz = data.ballisticDropOffsetAz;
    state.ballisticDropOffsetEl = data.ballisticDropOffsetEl;
    // LAC
    state.leadAngleCompensationActive = data.leadAngleCompensationActive;
    state.currentLeadAngleStatus = data.currentLeadAngleStatus;
    state.motionLeadOffsetAz = data.motionLeadOffsetAz;
    state.motionLeadOffsetEl = data.motionLeadOffsetEl;
    state.lacArmed = data.lacArmed;
    state.lacLatchedAzRate_dps = data.lacLatchedAzRate_dps;
    state.lacLatchedElRate_dps = data.lacLatchedElRate_dps;
    // Target parameters
    state.currentTargetRange = data.currentTargetRange;
    state.muzzleVelocityMPS = data.muzzleVelocityMPS;
    return state;
}

#endif // STATEPARTITIONS_H

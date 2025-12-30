/**
 * @file SafetyInterlock.h
 * @brief SKELETON: Centralized safety authority for RCWS weapon and motion control
 *
 * ARCHITECTURE REVIEW RECOMMENDATION:
 * This class should be the SINGLE AUTHORITY for all safety-critical decisions.
 * All weapon fire requests and motion commands should route through this class.
 *
 * DESIGN PRINCIPLES:
 * 1. Single Responsibility: Only safety decisions, no control logic
 * 2. Fail-Safe: Default state is SAFE (no fire, no motion)
 * 3. Auditable: All state transitions logged with timestamps
 * 4. Deterministic: Bounded response time for E-Stop
 * 5. Immutable from most components: Only PLC/hardware can change safety inputs
 *
 * INTEGRATION POINTS (see SafetyIntegrationPoints.md):
 * - WeaponController: Must call canFire() before any fire command
 * - GimbalController: Must call canMove() before motion commands
 * - GimbalMotionModeBase: checkSafetyConditions() should delegate here
 * - SystemStateModel: Safety state changes should route through this class
 *
 * @author Architecture Review
 * @date 2025-12-30
 * @version SKELETON - NOT FOR PRODUCTION USE
 */

#ifndef SAFETYINTERLOCK_H
#define SAFETYINTERLOCK_H

#include <QObject>
#include <QDateTime>
#include <QMutex>

// Forward declarations
class SystemStateModel;

/**
 * @brief Safety state snapshot for audit logging
 */
struct SafetyState {
    // === HARDWARE INPUTS (from PLC21/PLC42) ===
    bool emergencyStopActive = true;      ///< E-Stop pressed (default SAFE)
    bool deadManSwitchActive = false;     ///< Dead man switch held
    bool stationEnabled = false;          ///< Station master enable
    bool hatchOpen = false;               ///< Hatch state sensor

    // === OPERATOR INPUTS ===
    bool gunArmed = false;                ///< Gun ARM switch position
    bool authorized = false;              ///< System authorization key

    // === DERIVED STATES ===
    bool inNoFireZone = false;            ///< Current position in no-fire zone
    bool inNoTraverseZone = false;        ///< Current position in no-traverse zone
    bool chargeCycleActive = false;       ///< Charging sequence in progress
    bool homingActive = false;            ///< Homing sequence in progress

    // === LIMITS ===
    bool upperElevationLimit = false;     ///< Upper elevation limit switch
    bool lowerElevationLimit = false;     ///< Lower elevation limit switch

    // === SYSTEM HEALTH ===
    bool plc21Connected = false;          ///< PLC21 communication OK
    bool plc42Connected = false;          ///< PLC42 communication OK
    bool servosFaulted = false;           ///< Any servo in fault state

    // Timestamp for audit
    QDateTime timestamp;

    bool operator==(const SafetyState& other) const;
    bool operator!=(const SafetyState& other) const { return !(*this == other); }
};

/**
 * @brief Reason codes for safety denials (for logging/display)
 */
enum class SafetyDenialReason {
    None = 0,
    EmergencyStopActive,
    DeadManSwitchNotHeld,
    StationDisabled,
    GunNotArmed,
    NotAuthorized,
    InNoFireZone,
    InNoTraverseZone,
    ChargingInProgress,
    HomingInProgress,
    ElevationLimitReached,
    PlcCommunicationLost,
    ServoFault,
    HatchOpen,
    MultipleReasons
};

/**
 * @class SafetyInterlock
 * @brief THE single authority for safety-critical decisions
 *
 * USAGE:
 * @code
 * // In WeaponController before firing:
 * SafetyDenialReason reason;
 * if (!m_safetyInterlock->canFire(&reason)) {
 *     logDenial("Fire request denied", reason);
 *     return;
 * }
 * // Proceed with fire command...
 *
 * // In GimbalMotionModeBase before motion:
 * if (!m_safetyInterlock->canMove(azVelocity, elVelocity)) {
 *     stopServos();
 *     return;
 * }
 * @endcode
 */
class SafetyInterlock : public QObject
{
    Q_OBJECT

public:
    explicit SafetyInterlock(QObject* parent = nullptr);
    ~SafetyInterlock() override;

    // ========================================================================
    // CORE SAFETY QUERIES - These are the primary interface
    // ========================================================================

    /**
     * @brief Check if weapon fire is permitted
     * @param outReason Optional pointer to receive denial reason
     * @return true if ALL safety conditions for firing are met
     *
     * Checks:
     * - emergencyStopActive == false
     * - deadManSwitchActive == true
     * - stationEnabled == true
     * - gunArmed == true
     * - authorized == true
     * - inNoFireZone == false
     * - chargeCycleActive == false
     * - plc21Connected == true
     */
    bool canFire(SafetyDenialReason* outReason = nullptr) const;

    /**
     * @brief Check if gimbal motion is permitted
     * @param requestedAzVelocity Requested azimuth velocity (deg/s)
     * @param requestedElVelocity Requested elevation velocity (deg/s)
     * @param outReason Optional pointer to receive denial reason
     * @return true if motion is permitted
     *
     * Checks:
     * - emergencyStopActive == false
     * - stationEnabled == true
     * - inNoTraverseZone == false (for requested direction)
     * - Elevation limits respected
     * - plc42Connected == true
     * - servosFaulted == false
     */
    bool canMove(double requestedAzVelocity,
                 double requestedElVelocity,
                 SafetyDenialReason* outReason = nullptr) const;

    /**
     * @brief Check if tracking engagement is permitted
     * @param outReason Optional pointer to receive denial reason
     * @return true if tracking can be engaged
     */
    bool canEngage(SafetyDenialReason* outReason = nullptr) const;

    /**
     * @brief Check if homing sequence can start
     * @param outReason Optional pointer to receive denial reason
     * @return true if homing is permitted
     */
    bool canHome(SafetyDenialReason* outReason = nullptr) const;

    // ========================================================================
    // STATE ACCESSORS (read-only)
    // ========================================================================

    /**
     * @brief Get current safety state snapshot
     * @return Copy of current SafetyState
     */
    SafetyState currentState() const;

    /**
     * @brief Check if emergency stop is active
     * @return true if E-Stop is pressed
     */
    bool isEmergencyStopActive() const;

    /**
     * @brief Check if system is in a safe idle state
     * @return true if no hazardous operations are possible
     */
    bool isSafeIdle() const;

    /**
     * @brief Get human-readable description of denial reason
     * @param reason The denial reason code
     * @return Localized string describing the reason
     */
    static QString denialReasonToString(SafetyDenialReason reason);

    // ========================================================================
    // STATE UPDATE METHODS (called by hardware interfaces only)
    // ========================================================================

    /**
     * @brief Update safety state from PLC21 data
     * @param emergencyStop E-Stop state from PLC21
     * @param gunArmed Gun ARM switch state
     * @param authorized Authorization key state
     * @param deadManSwitch Dead man switch state
     * @param stationEnabled Station enable switch
     *
     * IMPORTANT: Only HardwareManager/Plc21DataModel should call this
     */
    void updateFromPlc21(bool emergencyStop,
                         bool gunArmed,
                         bool authorized,
                         bool deadManSwitch,
                         bool stationEnabled);

    /**
     * @brief Update safety state from PLC42 data
     * @param upperLimit Upper elevation limit
     * @param lowerLimit Lower elevation limit
     * @param hatchState Hatch open/closed state
     * @param plcConnected PLC42 communication status
     *
     * IMPORTANT: Only HardwareManager/Plc42DataModel should call this
     */
    void updateFromPlc42(bool upperLimit,
                         bool lowerLimit,
                         bool hatchState,
                         bool plcConnected);

    /**
     * @brief Update zone-based safety state
     * @param inNoFireZone Current position in no-fire zone
     * @param inNoTraverseZone Current position in no-traverse zone
     *
     * IMPORTANT: Only ZoneEnforcementService should call this
     */
    void updateZoneState(bool inNoFireZone, bool inNoTraverseZone);

    /**
     * @brief Update charging/homing sequence state
     * @param chargingActive Charging sequence in progress
     * @param homingActive Homing sequence in progress
     */
    void updateSequenceState(bool chargingActive, bool homingActive);

    /**
     * @brief Update servo health state
     * @param azFault Azimuth servo fault
     * @param elFault Elevation servo fault
     * @param actuatorFault Actuator fault
     */
    void updateServoHealth(bool azFault, bool elFault, bool actuatorFault);

signals:
    /**
     * @brief Emitted when any safety state changes
     * @param newState The new safety state
     * @param oldState The previous safety state
     */
    void safetyStateChanged(const SafetyState& newState, const SafetyState& oldState);

    /**
     * @brief Emitted when emergency stop state changes
     * @param active true if E-Stop is now active
     */
    void emergencyStopChanged(bool active);

    /**
     * @brief Emitted when fire permission changes
     * @param canFire true if fire is now permitted
     * @param reason Reason if fire is denied
     */
    void firePermissionChanged(bool canFire, SafetyDenialReason reason);

    /**
     * @brief Emitted when motion permission changes
     * @param canMove true if motion is now permitted
     * @param reason Reason if motion is denied
     */
    void motionPermissionChanged(bool canMove, SafetyDenialReason reason);

private:
    /**
     * @brief Log safety state transition for audit trail
     * @param oldState Previous state
     * @param newState New state
     * @param source Source of the change (e.g., "PLC21", "Zone")
     */
    void logStateTransition(const SafetyState& oldState,
                            const SafetyState& newState,
                            const QString& source);

    /**
     * @brief Emit appropriate signals after state change
     * @param oldState Previous state
     * @param newState New state
     */
    void emitStateChangeSignals(const SafetyState& oldState,
                                 const SafetyState& newState);

    mutable QMutex m_mutex;  ///< Thread safety for state access
    SafetyState m_state;     ///< Current safety state
};

#endif // SAFETYINTERLOCK_H

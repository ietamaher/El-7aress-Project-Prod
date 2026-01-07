#pragma once

/**
 * @file SafetyInterlock.h
 * @brief Centralized safety authority for RCWS weapon and motion control
 *
 * This class is the SINGLE AUTHORITY for all safety-critical decisions.
 * All weapon fire requests, motion commands, and charging operations
 * must route through this class.
 *
 * DESIGN PRINCIPLES:
 * 1. Single Responsibility: Only safety decisions, no control logic
 * 2. Fail-Safe: Default state is SAFE (no fire, no motion, no charging)
 * 3. Auditable: All state transitions logged with timestamps
 * 4. Deterministic: Bounded response time for E-Stop
 * 5. Immutable from most components: Only PLC/hardware can change safety inputs
 *
 * INTEGRATION POINTS:
 * - WeaponController: Must call canFire() before fire command
 * - WeaponController: Must call canCharge() before cocking actuator operation
 * - GimbalMotionModeBase: checkSafetyConditions() delegates to canMove()
 * - SystemStateModel: Safety state updates route through this class
 *
 * @date 2025-12-30
 * @version 1.0
 */

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>
#include <QDateTime>
#include <QMutex>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
class SystemStateModel;

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
    ChargeLockoutActive,  ///< 4-second lockout after charge completion (CROWS M153)
    ChargeFaultActive,    ///< Charging in fault state
    HomingInProgress,
    ElevationLimitReached,
    PlcCommunicationLost,
    ServoFault,
    ActuatorFault,
    HatchOpen,
    OperationalModeInvalid,
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
 *     qWarning() << "Fire denied:" << SafetyInterlock::denialReasonToString(reason);
 *     return;
 * }
 *
 * // In WeaponController before charging:
 * if (!m_safetyInterlock->canCharge(&reason)) {
 *     qWarning() << "Charge denied:" << SafetyInterlock::denialReasonToString(reason);
 *     return;
 * }
 *
 * // In GimbalMotionModeBase before motion:
 * if (!m_safetyInterlock->canMove(MotionMode::Manual, &reason)) {
 *     stopServos();
 *     return;
 * }
 * @endcode
 */
class SafetyInterlock : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct SafetyInterlock with SystemStateModel dependency
     * @param stateModel Pointer to SystemStateModel for state access
     * @param parent Parent QObject
     */
    explicit SafetyInterlock(SystemStateModel* stateModel, QObject* parent = nullptr);
    ~SafetyInterlock() override;

    // ============================================================================
    // CORE SAFETY QUERIES - Primary interface for safety decisions
    // ============================================================================

    /**
     * @brief Check if weapon fire is permitted
     * @param outReason Optional pointer to receive denial reason
     * @return true if ALL safety conditions for firing are met
     *
     * Requirements (ALL must be true):
     * - emergencyStopActive == false
     * - deadManSwitchActive == true
     * - stationEnabled == true
     * - gunArmed == true
     * - authorized == true
     * - inNoFireZone == false
     * - chargeCycleActive == false (weapon must be charged and ready)
     * - chargeLockoutActive == false (4-second lockout expired)
     */
    bool canFire(SafetyDenialReason* outReason = nullptr) const;

    /**
     * @brief Check if cocking actuator charging is permitted
     * @param outReason Optional pointer to receive denial reason
     * @return true if charging can proceed
     *
     * Requirements (ALL must be true):
     * - emergencyStopActive == false
     * - stationEnabled == true
     * - chargeLockoutActive == false (4-second lockout expired)
     * - chargeFaultActive == false (no uncleared fault)
     * - chargeCycleActive == false (not already charging)
     *
     * Note: Does NOT require gunArmed or deadManSwitch
     */
    bool canCharge(SafetyDenialReason* outReason = nullptr) const;

    /**
     * @brief Check if gimbal motion is permitted
     * @param currentMode Current motion mode (affects dead man switch requirement)
     * @param outReason Optional pointer to receive denial reason
     * @return true if motion is permitted
     *
     * Requirements:
     * - emergencyStopActive == false
     * - stationEnabled == true
     * - For Manual/AutoTrack: deadManSwitchActive == true
     * - servosFaulted == false
     */
    bool canMove(int motionMode, SafetyDenialReason* outReason = nullptr) const;

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

    // ============================================================================
    // STATE ACCESSORS (read-only)
    // ============================================================================

    /**
     * @brief Check if emergency stop is active
     * @return true if E-Stop is pressed
     */
    bool isEmergencyStopActive() const;

    /**
     * @brief Check if station is enabled
     * @return true if station master enable is active
     */
    bool isStationEnabled() const;

    /**
     * @brief Check if dead man switch is held
     * @return true if dead man switch is active
     */
    bool isDeadManSwitchActive() const;

    /**
     * @brief Check if gun is armed
     * @return true if gun ARM switch is in armed position
     */
    bool isGunArmed() const;

    /**
     * @brief Check if system is in a safe idle state
     * @return true if no hazardous operations are possible
     */
    bool isSafeIdle() const;

    /**
     * @brief Check if currently in no-fire zone
     * @return true if reticle is in a no-fire zone
     */
    bool isInNoFireZone() const;

    /**
     * @brief Check if currently in no-traverse zone
     * @return true if reticle is in a no-traverse zone
     */
    bool isInNoTraverseZone() const;

    /**
     * @brief Get human-readable description of denial reason
     * @param reason The denial reason code
     * @return String describing the reason
     */
    static QString denialReasonToString(SafetyDenialReason reason);

signals:
    /**
     * @brief Emitted when emergency stop state changes
     * @param active true if E-Stop is now active
     */
    void emergencyStopChanged(bool active);

    /**
     * @brief Emitted when fire permission changes
     * @param permitted true if fire is now permitted
     * @param reason Reason if fire is denied
     */
    void firePermissionChanged(bool permitted, SafetyDenialReason reason);

    /**
     * @brief Emitted when charge permission changes
     * @param permitted true if charging is now permitted
     * @param reason Reason if charging is denied
     */
    void chargePermissionChanged(bool permitted, SafetyDenialReason reason);

    /**
     * @brief Emitted when motion permission changes
     * @param permitted true if motion is now permitted
     * @param reason Reason if motion is denied
     */
    void motionPermissionChanged(bool permitted, SafetyDenialReason reason);

private:
    // ============================================================================
    // AUDIT LOGGING HELPERS
    // ============================================================================
    void logAuditEvent(const QString& operation, bool permitted, SafetyDenialReason reason) const;

    SystemStateModel* m_stateModel = nullptr;
    mutable QMutex m_mutex;

    // Cached previous state for change detection
    bool m_lastEmergencyStop = true;
    bool m_lastCanFire = false;
    bool m_lastCanCharge = false;
    bool m_lastCanMove = false;

    // ============================================================================
    // AUDIT LOGGING STATE (Rate-limited to prevent log spam)
    // ============================================================================
    mutable qint64 m_lastFireLogTime = 0;
    mutable qint64 m_lastChargeLogTime = 0;
    mutable qint64 m_lastMoveLogTime = 0;
    mutable SafetyDenialReason m_lastFireDenialReason = SafetyDenialReason::None;
    mutable SafetyDenialReason m_lastChargeDenialReason = SafetyDenialReason::None;
    mutable SafetyDenialReason m_lastMoveDenialReason = SafetyDenialReason::None;
};


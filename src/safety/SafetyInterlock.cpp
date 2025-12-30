/**
 * @file SafetyInterlock.cpp
 * @brief Implementation of centralized safety authority
 */

#include "SafetyInterlock.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

SafetyInterlock::SafetyInterlock(SystemStateModel* stateModel, QObject* parent)
    : QObject(parent)
    , m_stateModel(stateModel)
{
    if (!m_stateModel) {
        qCritical() << "[SafetyInterlock] CRITICAL: SystemStateModel is null!";
        qCritical() << "[SafetyInterlock] All safety queries will return DENY";
    } else {
        qInfo() << "[SafetyInterlock] Initialized - Safety authority active";
    }
}

SafetyInterlock::~SafetyInterlock()
{
    qInfo() << "[SafetyInterlock] Destroyed";
}

// ============================================================================
// CORE SAFETY QUERIES
// ============================================================================

bool SafetyInterlock::canFire(SafetyDenialReason* outReason) const
{
    QMutexLocker locker(&m_mutex);

    if (outReason) {
        *outReason = SafetyDenialReason::None;
    }

    // Fail-safe: deny if no state model
    if (!m_stateModel) {
        if (outReason) *outReason = SafetyDenialReason::PlcCommunicationLost;
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // Priority order matches CROWS M153 safety hierarchy

    // 1. Emergency stop - highest priority
    if (data.emergencyStopActive) {
        if (outReason) *outReason = SafetyDenialReason::EmergencyStopActive;
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        if (outReason) *outReason = SafetyDenialReason::StationDisabled;
        return false;
    }

    // 3. Dead man switch must be held
    if (!data.deadManSwitchActive) {
        if (outReason) *outReason = SafetyDenialReason::DeadManSwitchNotHeld;
        return false;
    }

    // 4. Gun must be armed
    if (!data.gunArmed) {
        if (outReason) *outReason = SafetyDenialReason::GunNotArmed;
        return false;
    }

    // 5. System must be authorized
    if (!data.authorized) {
        if (outReason) *outReason = SafetyDenialReason::NotAuthorized;
        return false;
    }

    // 6. Must not be in no-fire zone
    if (data.isReticleInNoFireZone) {
        if (outReason) *outReason = SafetyDenialReason::InNoFireZone;
        return false;
    }

    // 7. Must not be actively charging
    if (data.chargingState == ChargingState::Extending ||
        data.chargingState == ChargingState::Retracting) {
        if (outReason) *outReason = SafetyDenialReason::ChargingInProgress;
        return false;
    }

    // 8. Must not be in lockout period (4-second CROWS spec)
    if (data.chargingState == ChargingState::Lockout) {
        if (outReason) *outReason = SafetyDenialReason::ChargeLockoutActive;
        return false;
    }

    // 9. Must not be in fault state
    if (data.chargingState == ChargingState::Fault ||
        data.chargingState == ChargingState::JamDetected) {
        if (outReason) *outReason = SafetyDenialReason::ChargeFaultActive;
        return false;
    }

    // All checks passed
    return true;
}

bool SafetyInterlock::canCharge(SafetyDenialReason* outReason) const
{
    QMutexLocker locker(&m_mutex);

    if (outReason) {
        *outReason = SafetyDenialReason::None;
    }

    // Fail-safe: deny if no state model
    if (!m_stateModel) {
        if (outReason) *outReason = SafetyDenialReason::PlcCommunicationLost;
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // 1. Emergency stop - highest priority
    if (data.emergencyStopActive) {
        if (outReason) *outReason = SafetyDenialReason::EmergencyStopActive;
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        if (outReason) *outReason = SafetyDenialReason::StationDisabled;
        return false;
    }

    // 3. Must not already be in a charging cycle
    if (data.chargingState == ChargingState::Extending ||
        data.chargingState == ChargingState::Retracting ||
        data.chargingState == ChargingState::SafeRetract) {
        if (outReason) *outReason = SafetyDenialReason::ChargingInProgress;
        return false;
    }

    // 4. Must not be in lockout period (4-second CROWS spec after charge)
    if (data.chargingState == ChargingState::Lockout) {
        if (outReason) *outReason = SafetyDenialReason::ChargeLockoutActive;
        return false;
    }

    // 5. Must not be in fault state (requires explicit reset)
    if (data.chargingState == ChargingState::Fault ||
        data.chargingState == ChargingState::JamDetected) {
        if (outReason) *outReason = SafetyDenialReason::ChargeFaultActive;
        return false;
    }

    // Note: canCharge does NOT require:
    // - gunArmed (can charge with gun in SAFE)
    // - deadManSwitch (charging is not a firing operation)
    // - authorized (charging is a preparation step)

    // All checks passed
    return true;
}

bool SafetyInterlock::canMove(int motionMode, SafetyDenialReason* outReason) const
{
    QMutexLocker locker(&m_mutex);

    if (outReason) {
        *outReason = SafetyDenialReason::None;
    }

    // Fail-safe: deny if no state model
    if (!m_stateModel) {
        if (outReason) *outReason = SafetyDenialReason::PlcCommunicationLost;
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // 1. Emergency stop - highest priority
    if (data.emergencyStopActive) {
        if (outReason) *outReason = SafetyDenialReason::EmergencyStopActive;
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        if (outReason) *outReason = SafetyDenialReason::StationDisabled;
        return false;
    }

    // 3. Dead man switch required for Manual and AutoTrack modes
    // MotionMode::Manual = 1, MotionMode::AutoTrack = 2
    if (motionMode == 1 || motionMode == 2) {
        if (!data.deadManSwitchActive) {
            if (outReason) *outReason = SafetyDenialReason::DeadManSwitchNotHeld;
            return false;
        }
    }

    // All checks passed
    return true;
}

bool SafetyInterlock::canEngage(SafetyDenialReason* outReason) const
{
    QMutexLocker locker(&m_mutex);

    if (outReason) {
        *outReason = SafetyDenialReason::None;
    }

    if (!m_stateModel) {
        if (outReason) *outReason = SafetyDenialReason::PlcCommunicationLost;
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // 1. Emergency stop check
    if (data.emergencyStopActive) {
        if (outReason) *outReason = SafetyDenialReason::EmergencyStopActive;
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        if (outReason) *outReason = SafetyDenialReason::StationDisabled;
        return false;
    }

    // 3. Dead man switch required for engagement
    if (!data.deadManSwitchActive) {
        if (outReason) *outReason = SafetyDenialReason::DeadManSwitchNotHeld;
        return false;
    }

    return true;
}

bool SafetyInterlock::canHome(SafetyDenialReason* outReason) const
{
    QMutexLocker locker(&m_mutex);

    if (outReason) {
        *outReason = SafetyDenialReason::None;
    }

    if (!m_stateModel) {
        if (outReason) *outReason = SafetyDenialReason::PlcCommunicationLost;
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // 1. Emergency stop check
    if (data.emergencyStopActive) {
        if (outReason) *outReason = SafetyDenialReason::EmergencyStopActive;
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        if (outReason) *outReason = SafetyDenialReason::StationDisabled;
        return false;
    }

    // 3. Must not already be homing
    if (data.homingState == HomingState::InProgress ||
        data.homingState == HomingState::Requested) {
        if (outReason) *outReason = SafetyDenialReason::HomingInProgress;
        return false;
    }

    return true;
}

// ============================================================================
// STATE ACCESSORS
// ============================================================================

bool SafetyInterlock::isEmergencyStopActive() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_stateModel) return true;  // Fail-safe
    return m_stateModel->data().emergencyStopActive;
}

bool SafetyInterlock::isStationEnabled() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_stateModel) return false;  // Fail-safe
    return m_stateModel->data().stationEnabled;
}

bool SafetyInterlock::isDeadManSwitchActive() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_stateModel) return false;  // Fail-safe
    return m_stateModel->data().deadManSwitchActive;
}

bool SafetyInterlock::isGunArmed() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_stateModel) return false;  // Fail-safe
    return m_stateModel->data().gunArmed;
}

bool SafetyInterlock::isSafeIdle() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_stateModel) return true;  // Fail-safe: no actions possible

    const SystemStateData& data = m_stateModel->data();

    // Safe idle = E-stop active OR station disabled OR gun not armed
    return data.emergencyStopActive || !data.stationEnabled || !data.gunArmed;
}

bool SafetyInterlock::isInNoFireZone() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_stateModel) return true;  // Fail-safe: assume in no-fire zone
    return m_stateModel->data().isReticleInNoFireZone;
}

bool SafetyInterlock::isInNoTraverseZone() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_stateModel) return true;  // Fail-safe: assume in no-traverse zone
    return m_stateModel->data().isReticleInNoTraverseZone;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

QString SafetyInterlock::denialReasonToString(SafetyDenialReason reason)
{
    switch (reason) {
    case SafetyDenialReason::None:
        return "No denial";
    case SafetyDenialReason::EmergencyStopActive:
        return "EMERGENCY STOP ACTIVE";
    case SafetyDenialReason::DeadManSwitchNotHeld:
        return "Dead man switch not held";
    case SafetyDenialReason::StationDisabled:
        return "Station not enabled";
    case SafetyDenialReason::GunNotArmed:
        return "Gun not armed";
    case SafetyDenialReason::NotAuthorized:
        return "System not authorized";
    case SafetyDenialReason::InNoFireZone:
        return "In NO-FIRE zone";
    case SafetyDenialReason::InNoTraverseZone:
        return "In NO-TRAVERSE zone";
    case SafetyDenialReason::ChargingInProgress:
        return "Charging in progress";
    case SafetyDenialReason::ChargeLockoutActive:
        return "Charge lockout active (wait 4 sec)";
    case SafetyDenialReason::ChargeFaultActive:
        return "Charge fault - reset required";
    case SafetyDenialReason::HomingInProgress:
        return "Homing in progress";
    case SafetyDenialReason::ElevationLimitReached:
        return "Elevation limit reached";
    case SafetyDenialReason::PlcCommunicationLost:
        return "PLC communication lost";
    case SafetyDenialReason::ServoFault:
        return "Servo fault detected";
    case SafetyDenialReason::ActuatorFault:
        return "Actuator fault detected";
    case SafetyDenialReason::HatchOpen:
        return "Hatch is open";
    case SafetyDenialReason::OperationalModeInvalid:
        return "Invalid operational mode";
    case SafetyDenialReason::MultipleReasons:
        return "Multiple safety violations";
    default:
        return QString("Unknown reason (%1)").arg(static_cast<int>(reason));
    }
}

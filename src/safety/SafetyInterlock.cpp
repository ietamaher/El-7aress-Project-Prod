/**
 * @file SafetyInterlock.cpp
 * @brief Implementation of centralized safety authority
 */

#include "SafetyInterlock.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QDateTime>

// ============================================================================
// AUDIT LOGGING CONFIGURATION
// ============================================================================
// Rate-limited logging to prevent log spam while maintaining audit trail
// At 20Hz control loop, this allows 1 log per 5 seconds per query type
static constexpr int AUDIT_LOG_INTERVAL_MS = 5000;

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

    SafetyDenialReason reason = SafetyDenialReason::None;
    bool permitted = false;

    // Fail-safe: deny if no state model
    if (!m_stateModel) {
        reason = SafetyDenialReason::PlcCommunicationLost;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // Priority order matches CROWS M153 safety hierarchy

    // 1. Emergency stop - highest priority
    if (data.emergencyStopActive) {
        reason = SafetyDenialReason::EmergencyStopActive;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        reason = SafetyDenialReason::StationDisabled;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 3. Dead man switch must be held
    if (!data.deadManSwitchActive) {
        reason = SafetyDenialReason::DeadManSwitchNotHeld;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 4. Gun must be armed
    if (!data.gunArmed) {
        reason = SafetyDenialReason::GunNotArmed;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 5. System must be in Engagement mode (CROWS fire control requirement)
    if (data.opMode != OperationalMode::Engagement) {
        reason = SafetyDenialReason::OperationalModeInvalid;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 6. System must be authorized
    if (!data.authorized) {
        reason = SafetyDenialReason::NotAuthorized;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 7. Must not be in no-fire zone
    if (data.isReticleInNoFireZone) {
        reason = SafetyDenialReason::InNoFireZone;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 8. Must not be actively charging
    if (data.chargingState == ChargingState::Extending ||
        data.chargingState == ChargingState::Retracting ||
        data.chargingState == ChargingState::SafeRetract ||
        data.chargingState == ChargingState::Extended) {
        reason = SafetyDenialReason::ChargingInProgress;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // 9. Must not be in fault state
    if (data.chargingState == ChargingState::Fault ||
        data.chargingState == ChargingState::JamDetected) {
        reason = SafetyDenialReason::ChargeFaultActive;
        if (outReason) *outReason = reason;
        logAuditEvent("FIRE", false, reason);
        return false;
    }

    // All checks passed
    if (outReason) *outReason = SafetyDenialReason::None;
    permitted = true;
    logAuditEvent("FIRE", true, SafetyDenialReason::None);
    return true;
}

bool SafetyInterlock::canCharge(SafetyDenialReason* outReason) const
{
    QMutexLocker locker(&m_mutex);

    SafetyDenialReason reason = SafetyDenialReason::None;

    // Fail-safe: deny if no state model
    if (!m_stateModel) {
        reason = SafetyDenialReason::PlcCommunicationLost;
        if (outReason) *outReason = reason;
        logAuditEvent("CHARGE", false, reason);
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // 1. Emergency stop - highest priority
    if (data.emergencyStopActive) {
        reason = SafetyDenialReason::EmergencyStopActive;
        if (outReason) *outReason = reason;
        logAuditEvent("CHARGE", false, reason);
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        reason = SafetyDenialReason::StationDisabled;
        if (outReason) *outReason = reason;
        logAuditEvent("CHARGE", false, reason);
        return false;
    }

    // 3. Must not already be in a charging cycle
    if (data.chargingState == ChargingState::Extending ||
        data.chargingState == ChargingState::Retracting ||
        data.chargingState == ChargingState::SafeRetract ||
        data.chargingState == ChargingState::Extended) {
        reason = SafetyDenialReason::ChargingInProgress;
        if (outReason) *outReason = reason;
        logAuditEvent("CHARGE", false, reason);
        return false;
    }

    // 4. Must not be in lockout period (4-second CROWS spec after charge)
    if (data.chargingState == ChargingState::Lockout) {
        reason = SafetyDenialReason::ChargeLockoutActive;
        if (outReason) *outReason = reason;
        logAuditEvent("CHARGE", false, reason);
        return false;
    }

    // 5. Must not be in fault state (requires explicit reset)
    if (data.chargingState == ChargingState::Fault ||
        data.chargingState == ChargingState::JamDetected) {
        reason = SafetyDenialReason::ChargeFaultActive;
        if (outReason) *outReason = reason;
        logAuditEvent("CHARGE", false, reason);
        return false;
    }

    // Note: canCharge does NOT require:
    // - gunArmed (can charge with gun in SAFE)
    // - deadManSwitch (charging is not a firing operation)
    // - authorized (charging is a preparation step)

    // All checks passed
    if (outReason) *outReason = SafetyDenialReason::None;
    logAuditEvent("CHARGE", true, SafetyDenialReason::None);
    return true;
}

bool SafetyInterlock::canMove(int motionMode, SafetyDenialReason* outReason) const
{
    QMutexLocker locker(&m_mutex);

    SafetyDenialReason reason = SafetyDenialReason::None;

    // Fail-safe: deny if no state model
    if (!m_stateModel) {
        reason = SafetyDenialReason::PlcCommunicationLost;
        if (outReason) *outReason = reason;
        logAuditEvent("MOVE", false, reason);
        return false;
    }

    const SystemStateData& data = m_stateModel->data();

    // 1. Emergency stop - highest priority
    if (data.emergencyStopActive) {
        reason = SafetyDenialReason::EmergencyStopActive;
        if (outReason) *outReason = reason;
        logAuditEvent("MOVE", false, reason);
        return false;
    }

    // 2. Station must be enabled
    if (!data.stationEnabled) {
        reason = SafetyDenialReason::StationDisabled;
        if (outReason) *outReason = reason;
        logAuditEvent("MOVE", false, reason);
        return false;
    }

    // 3. Dead man switch required for Manual and AutoTrack modes
    // MotionMode enum: Manual=0, Pattern=1, AutoTrack=2, ManualTrack=3
    // ⭐ BUG FIX: Was checking motionMode==1 (Pattern) instead of motionMode==0 (Manual)
    if (motionMode == static_cast<int>(MotionMode::Manual) ||
        motionMode == static_cast<int>(MotionMode::AutoTrack) ||
        motionMode == static_cast<int>(MotionMode::ManualTrack)) {
        if (!data.deadManSwitchActive) {
            reason = SafetyDenialReason::DeadManSwitchNotHeld;
            if (outReason) *outReason = reason;
            logAuditEvent("MOVE", false, reason);
            return false;
        }
    }

    // All checks passed
    if (outReason) *outReason = SafetyDenialReason::None;
    logAuditEvent("MOVE", true, SafetyDenialReason::None);
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

// ============================================================================
// AUDIT LOGGING
// ============================================================================

void SafetyInterlock::logAuditEvent(const QString& operation, bool permitted,
                                     SafetyDenialReason reason) const
{
    // ========================================================================
    // RATE-LIMITED AUDIT LOGGING FOR CERTIFICATION TRACEABILITY
    // ========================================================================
    // This function logs safety decisions with timestamps for audit trail.
    // Rate limiting prevents log spam while ensuring state changes are captured.
    //
    // Logging strategy:
    // 1. Always log permission changes (GRANTED <-> DENIED)
    // 2. Always log denial reason changes (different failure mode)
    // 3. Rate-limit repeated denials with same reason to 1 per 5 seconds
    // ========================================================================

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64* lastLogTime = nullptr;
    SafetyDenialReason* lastReason = nullptr;

    // Select the appropriate state variables based on operation type
    if (operation == "FIRE") {
        lastLogTime = &m_lastFireLogTime;
        lastReason = &m_lastFireDenialReason;
    } else if (operation == "CHARGE") {
        lastLogTime = &m_lastChargeLogTime;
        lastReason = &m_lastChargeDenialReason;
    } else if (operation == "MOVE") {
        lastLogTime = &m_lastMoveLogTime;
        lastReason = &m_lastMoveDenialReason;
    } else {
        // Unknown operation - always log
        QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        qInfo() << QString("[SafetyInterlock AUDIT] %1 | %2: %3 | Reason: %4")
                      .arg(timestamp)
                      .arg(operation)
                      .arg(permitted ? "PERMITTED" : "DENIED")
                      .arg(denialReasonToString(reason));
        return;
    }

    // Determine if we should log this event
    bool shouldLog = false;

    if (permitted) {
        // Permission granted - only log if previously denied
        if (*lastReason != SafetyDenialReason::None) {
            shouldLog = true;
        }
    } else {
        // Permission denied
        if (*lastReason != reason) {
            // Denial reason changed - always log (different failure mode)
            shouldLog = true;
        } else if ((currentTime - *lastLogTime) >= AUDIT_LOG_INTERVAL_MS) {
            // Same reason but rate limit expired - log periodic reminder
            shouldLog = true;
        }
    }

    if (shouldLog) {
        QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);

        if (permitted) {
            qInfo() << QString("[SafetyInterlock AUDIT] %1 | %2: PERMITTED")
                          .arg(timestamp)
                          .arg(operation);
        } else {
            qWarning() << QString("[SafetyInterlock AUDIT] %1 | %2: DENIED | Reason: %3")
                             .arg(timestamp)
                             .arg(operation)
                             .arg(denialReasonToString(reason));
        }

        *lastLogTime = currentTime;
    }

    // Always update the last reason for change detection
    *lastReason = reason;
}

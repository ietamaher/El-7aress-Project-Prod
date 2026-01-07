/**
 * @file EmergencyStopMonitor.cpp
 * @brief Implementation of centralized emergency stop monitoring
 */

#include "EmergencyStopMonitor.h"
#include <QDebug>
#include <QTimer>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

EmergencyStopMonitor::EmergencyStopMonitor(QObject* parent) : QObject(parent) {
    m_stateTimer.start();
    m_debounceTimer.start();

    qInfo() << "[EmergencyStopMonitor] Initialized" << "| Debounce:" << DEBOUNCE_MS << "ms"
            << "| Recovery delay:" << RECOVERY_DELAY_MS << "ms";
}

// ============================================================================
// STATE UPDATE
// ============================================================================

void EmergencyStopMonitor::updateState(bool isActive, const QString& source) {
    // If state hasn't changed, nothing to do
    if (isActive == m_isActive && !m_isDebouncing) {
        return;
    }

    // Start debounce if this is a new state change
    if (!m_isDebouncing && isActive != m_isActive) {
        m_isDebouncing = true;
        m_pendingState = isActive;
        m_debounceTimer.restart();
        return;
    }

    // If debouncing, check if we've exceeded debounce time
    if (m_isDebouncing) {
        // If state changed back during debounce, cancel
        if (isActive != m_pendingState) {
            m_isDebouncing = false;
            return;
        }

        // Check if debounce period has elapsed
        if (m_debounceTimer.elapsed() >= DEBOUNCE_MS) {
            m_isDebouncing = false;
            processStateChange(m_pendingState, source);
        }
    }
}

void EmergencyStopMonitor::forceActivate(const QString& reason) {
    if (m_isActive) {
        qDebug() << "[EmergencyStopMonitor] Force activate ignored - already active";
        return;
    }

    qCritical() << "";
    qCritical() << "========================================";
    qCritical() << "  SOFTWARE EMERGENCY STOP TRIGGERED";
    qCritical() << "  Reason:" << reason;
    qCritical() << "========================================";
    qCritical() << "";

    // Bypass debounce for forced activation
    m_isDebouncing = false;
    processStateChange(true, "SOFTWARE: " + reason);
}

// ============================================================================
// STATE QUERIES
// ============================================================================

bool EmergencyStopMonitor::isInRecovery() const {
    if (m_isActive) {
        return false;  // Can't be in recovery while active
    }

    if (!m_lastDeactivationTime.isValid()) {
        return false;  // Never been deactivated
    }

    qint64 timeSinceDeactivation = m_lastDeactivationTime.msecsTo(QDateTime::currentDateTime());
    return timeSinceDeactivation < RECOVERY_DELAY_MS;
}

bool EmergencyStopMonitor::isDebouncing() const {
    return m_isDebouncing;
}

qint64 EmergencyStopMonitor::timeSinceLastChange() const {
    return m_stateTimer.elapsed();
}

qint64 EmergencyStopMonitor::activeDuration() const {
    if (!m_isActive) {
        return -1;
    }

    if (!m_lastActivationTime.isValid()) {
        return -1;
    }

    return m_lastActivationTime.msecsTo(QDateTime::currentDateTime());
}

// ============================================================================
// EVENT HISTORY
// ============================================================================

void EmergencyStopMonitor::clearHistory() {
    m_eventHistory.clear();
    qDebug() << "[EmergencyStopMonitor] Event history cleared";
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void EmergencyStopMonitor::processStateChange(bool newState, const QString& source) {
    if (newState == m_isActive) {
        return;  // No change
    }

    qint64 previousStateDuration = m_stateTimer.elapsed();
    m_stateTimer.restart();

    // Create event record
    EmergencyStopEvent event;
    event.timestamp = QDateTime::currentDateTime();
    event.wasActivation = newState;
    event.durationMs = previousStateDuration;
    event.source = source;

    // Update state
    bool wasActive = m_isActive;
    m_isActive = newState;

    if (newState) {
        // ACTIVATION
        m_activationCount++;
        m_lastActivationTime = event.timestamp;

        qCritical() << "";
        qCritical() << "========================================";
        qCritical() << "  EMERGENCY STOP ACTIVATED";
        qCritical() << "  Source:" << source;
        qCritical() << "  Time:" << event.timestamp.toString(Qt::ISODate);
        qCritical() << "  Activation #" << m_activationCount;
        qCritical() << "========================================";
        qCritical() << "";

        recordEvent(event);
        emit activated(event);

    } else {
        // DEACTIVATION
        m_lastDeactivationTime = event.timestamp;

        qInfo() << "";
        qInfo() << "========================================";
        qInfo() << "  EMERGENCY STOP CLEARED";
        qInfo() << "  Duration:" << previousStateDuration << "ms";
        qInfo() << "  Time:" << event.timestamp.toString(Qt::ISODate);
        qInfo() << "  Recovery period:" << RECOVERY_DELAY_MS << "ms";
        qInfo() << "========================================";
        qInfo() << "";

        recordEvent(event);
        emit deactivated(event);

        // Start recovery period
        emit recoveryStarted();

        // Schedule recovery complete signal
        QTimer::singleShot(RECOVERY_DELAY_MS, this, [this]() {
            // Only emit if still not active
            if (!m_isActive) {
                qInfo() << "[EmergencyStopMonitor] Recovery complete - normal operation permitted";
                emit recoveryComplete();
            }
        });
    }

    emit stateChanged(newState);
}

void EmergencyStopMonitor::recordEvent(const EmergencyStopEvent& event) {
    m_eventHistory.push_back(event);

    // Trim history if too large
    while (m_eventHistory.size() > MAX_EVENT_HISTORY) {
        m_eventHistory.erase(m_eventHistory.begin());
    }

    // Log for audit trail
    qInfo() << "[EmergencyStopMonitor] Event recorded:"
            << (event.wasActivation ? "ACTIVATED" : "DEACTIVATED") << "| Source:" << event.source
            << "| Duration:" << event.durationMs << "ms"
            << "| Total activations:" << m_activationCount;
}

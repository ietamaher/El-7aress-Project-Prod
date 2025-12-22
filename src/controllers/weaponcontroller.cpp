#include "weaponcontroller.h"

// ============================================================================
// Project
// ============================================================================
#include "models/domain/systemstatemodel.h"
#include "hardware/devices/servoactuatordevice.h"
#include "hardware/devices/plc42device.h"

// ============================================================================
// Qt Framework
// ============================================================================
#include <QDebug>

// ============================================================================
// Standard Library
// ============================================================================
#include <cmath>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

WeaponController::WeaponController(SystemStateModel* stateModel,
                                   ServoActuatorDevice* servoActuator,
                                   Plc42Device* plc42,
                                   QObject* parent)
    : QObject(parent)
    , m_stateModel(stateModel)
    , m_servoActuator(servoActuator)
    , m_plc42(plc42)
{
    setupTimers();
    setupConnections();

    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }

    // ========================================================================
    // BALLISTICS
    // ========================================================================
    m_ballisticsProcessor = new BallisticsProcessorLUT();
    bool lutLoaded = m_ballisticsProcessor->loadAmmunitionTable(":/ballistic/tables/m2_ball.json");
    if (lutLoaded) {
        qInfo() << "[WeaponController] Ballistic LUT loaded:" << m_ballisticsProcessor->getAmmunitionName();
    }

    // ========================================================================
    // JAM DETECTION CONFIG - LOG FOR OPERATOR
    // ========================================================================
    qInfo() << "╔════════════════════════════════════════════════════════════╗";
    qInfo() << "║     AMMUNITION FEED - JAM DETECTION ENABLED                ║";
    qInfo() << "╠════════════════════════════════════════════════════════════╣";
    qInfo() << "║  Torque Nominal:    " << QString("%1").arg(m_jamConfig.torqueNominal, 6) << " (CALIBRATE!)           ║";
    qInfo() << "║  Torque Warning:    " << QString("%1").arg(m_jamConfig.torqueWarning, 6) << "                        ║";
    qInfo() << "║  Torque Jam:        " << QString("%1").arg(m_jamConfig.torqueJamThreshold, 6) << "                        ║";
    qInfo() << "║  Torque Critical:   " << QString("%1").arg(m_jamConfig.torqueCritical, 6) << " (fuse protection)      ║";
    qInfo() << "║  Stall Time:        " << QString("%1").arg(m_jamConfig.stallTimeMs, 6) << " ms                      ║";
    qInfo() << "╚════════════════════════════════════════════════════════════╝";
}

WeaponController::~WeaponController()
{
    if (m_feedTimer) m_feedTimer->stop();
    if (m_safeRetractTimer) m_safeRetractTimer->stop();
    if (m_jamMonitorTimer) m_jamMonitorTimer->stop();

    delete m_ballisticsProcessor;
    m_ballisticsProcessor = nullptr;
}

// ============================================================================
// SETUP
// ============================================================================

void WeaponController::setupTimers()
{
    // Overall feed cycle timeout
    m_feedTimer = new QTimer(this);
    m_feedTimer->setSingleShot(true);
    connect(m_feedTimer, &QTimer::timeout, this, &WeaponController::onFeedTimeout);

    // Safe retract timeout
    m_safeRetractTimer = new QTimer(this);
    m_safeRetractTimer->setSingleShot(true);
    connect(m_safeRetractTimer, &QTimer::timeout, this, &WeaponController::onSafeRetractTimeout);

    // High-frequency jam monitoring (combines torque + stall check)
    m_jamMonitorTimer = new QTimer(this);
    m_jamMonitorTimer->setInterval(m_jamConfig.torqueSampleIntervalMs);
    connect(m_jamMonitorTimer, &QTimer::timeout, this, &WeaponController::onJamMonitorTimer);
}

void WeaponController::setupConnections()
{
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &WeaponController::onSystemStateChanged,
                Qt::DirectConnection);
    }

    if (m_servoActuator) {
        connect(m_servoActuator, &ServoActuatorDevice::actuatorDataChanged,
                this, &WeaponController::onActuatorFeedback,
                Qt::DirectConnection);
    }
}

// ============================================================================
// STATE CHANGE HANDLER
// ============================================================================

void WeaponController::onSystemStateChanged(const SystemStateData& newData)
{
    // ========================================================================
    // AMMO FEED CYCLE - NEW FSM-BASED APPROACH
    // Detect button PRESS (edge trigger, not level)
    // Button RELEASE is intentionally ignored - FSM controls the cycle
    // ========================================================================
    // ========================================================================
    // AMMO FEED BUTTON - PRESS DETECTION
    // ========================================================================
    if (newData.ammoLoadButtonPressed && !m_oldState.ammoLoadButtonPressed) {
        onOperatorRequestLoad();
    }

    // Handle button release
    if (!newData.ammoLoadButtonPressed && m_oldState.ammoLoadButtonPressed) {
        if (m_feedState == AmmoFeedState::Extended) {
            qDebug() << "[WeaponController] Button RELEASED in Extended - retracting";
            transitionFeedState(AmmoFeedState::Retracting);
            commandRetract();
            startJamMonitoring();
            m_feedTimer->start(FEED_TIMEOUT_MS);
        }
    }

    // ========================================================================
    // DEAD MAN SWITCH
    // ========================================================================
    if (m_oldState.deadManSwitchActive != newData.deadManSwitchActive) {
        m_fireReady = newData.deadManSwitchActive;
    }

    // ========================================================================
    // FIRE MODE SELECTION
    // ========================================================================
    if (m_oldState.fireMode != newData.fireMode) {
        switch (newData.fireMode) {
        case FireMode::SingleShot:
            m_plc42->setSolenoidMode(1);
            break;
        case FireMode::ShortBurst:
            m_plc42->setSolenoidMode(2);
            break;
        case FireMode::LongBurst:
            m_plc42->setSolenoidMode(3);
            break;
        default:
            m_plc42->setSolenoidMode(1);
            break;
        }
    }

    // ========================================================================
    // SYSTEM ARMING CONDITIONS
    // ========================================================================
    m_systemArmed = (newData.opMode == OperationalMode::Engagement &&
                     newData.gunArmed &&
                     m_fireReady);

    // ========================================================================
    // BALLISTICS RECALCULATION TRIGGERS
    // Detect changes in any parameter that affects lead angle calculation
    // ========================================================================
    bool ballisticsInputsChanged = false;

    // LAC activation/deactivation
    if (m_oldState.leadAngleCompensationActive != newData.leadAngleCompensationActive) {
        ballisticsInputsChanged = true;
        qDebug() << "[WeaponController] LAC state changed:"
                 << m_oldState.leadAngleCompensationActive << "->"
                 << newData.leadAngleCompensationActive;
    }

    // Camera switching (Day <-> Night) - affects which FOV is used
    if (m_oldState.activeCameraIsDay != newData.activeCameraIsDay) {
        ballisticsInputsChanged = true;
        QString oldCam = m_oldState.activeCameraIsDay ? "DAY" : "NIGHT";
        QString newCam = newData.activeCameraIsDay ? "DAY" : "NIGHT";
        float oldFov = m_oldState.activeCameraIsDay ? m_oldState.dayCurrentHFOV : m_oldState.nightCurrentHFOV;
        float newFov = newData.activeCameraIsDay ? newData.dayCurrentHFOV : newData.nightCurrentHFOV;
        qDebug() << "[WeaponController] CAMERA SWITCHED:" << oldCam << "->" << newCam
                 << "| Active FOV:" << oldFov << "deg ->" << newFov << "deg";
    }

    // FOV changes (zoom in/out)
    if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV) ||
        !qFuzzyCompare(m_oldState.dayCurrentVFOV, newData.dayCurrentVFOV) ||
        !qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV) ||
        !qFuzzyCompare(m_oldState.nightCurrentVFOV, newData.nightCurrentVFOV)) {
        ballisticsInputsChanged = true;

        if (!qFuzzyCompare(m_oldState.dayCurrentHFOV, newData.dayCurrentHFOV) ||
            !qFuzzyCompare(m_oldState.dayCurrentVFOV, newData.dayCurrentVFOV)) {
            qDebug() << "[WeaponController] Day camera ZOOM:"
                     << m_oldState.dayCurrentHFOV << "x" << m_oldState.dayCurrentVFOV << "deg ->"
                     << newData.dayCurrentHFOV << "x" << newData.dayCurrentVFOV << "deg"
                     << (newData.activeCameraIsDay ? "(ACTIVE)" : "(inactive)");
        }
        if (!qFuzzyCompare(m_oldState.nightCurrentHFOV, newData.nightCurrentHFOV) ||
            !qFuzzyCompare(m_oldState.nightCurrentVFOV, newData.nightCurrentVFOV)) {
            qDebug() << "[WeaponController] Night camera ZOOM:"
                     << m_oldState.nightCurrentHFOV << "x" << m_oldState.nightCurrentVFOV << "deg ->"
                     << newData.nightCurrentHFOV << "x" << newData.nightCurrentVFOV << "deg"
                     << (!newData.activeCameraIsDay ? "(ACTIVE)" : "(inactive)");
        }
    }

    // Target range changes (LRF measurement)
    if (!qFuzzyCompare(m_oldState.currentTargetRange, newData.currentTargetRange)) {
        ballisticsInputsChanged = true;
        /*qDebug() << "[WeaponController] Target range changed:"
                 << m_oldState.currentTargetRange << "->" << newData.currentTargetRange << "m";*/
    }

    // Target angular rates (tracking motion)
    if (!qFuzzyCompare(m_oldState.currentTargetAngularRateAz, newData.currentTargetAngularRateAz) ||
        !qFuzzyCompare(m_oldState.currentTargetAngularRateEl, newData.currentTargetAngularRateEl)) {
        ballisticsInputsChanged = true;
        /*qDebug() << "[WeaponController] Target angular rates changed: Az:"
                 << m_oldState.currentTargetAngularRateAz << "->" << newData.currentTargetAngularRateAz
                 << "El:" << m_oldState.currentTargetAngularRateEl << "->" << newData.currentTargetAngularRateEl;*/
    }

    // ========================================================================
    // ENVIRONMENTAL PARAMETERS CHANGE DETECTION
    // If environmental parameters changed and LAC is active, immediately
    // update ballistics processor with new conditions.
    // ========================================================================
    bool environmentalParamsChanged = false;

    if (!qFuzzyCompare(m_oldState.environmentalTemperatureCelsius, newData.environmentalTemperatureCelsius) ||
        !qFuzzyCompare(m_oldState.environmentalAltitudeMeters, newData.environmentalAltitudeMeters) ||
        m_oldState.environmentalAppliedToBallistics != newData.environmentalAppliedToBallistics) {
        environmentalParamsChanged = true;

        qDebug() << "[WeaponController] Environmental parameters changed:"
                 << "Temp:" << m_oldState.environmentalTemperatureCelsius << "->"
                 << newData.environmentalTemperatureCelsius
                 << "Alt:" << m_oldState.environmentalAltitudeMeters << "->"
                 << newData.environmentalAltitudeMeters
                 << "Applied:" << m_oldState.environmentalAppliedToBallistics << "->"
                 << newData.environmentalAppliedToBallistics;
    }

    // ========================================================================
    // WINDAGE PARAMETERS CHANGE DETECTION
    // Windage (wind direction + speed) affects crosswind calculation.
    // ========================================================================
    bool windageParamsChanged = false;

    if (!qFuzzyCompare(m_oldState.windageSpeedKnots, newData.windageSpeedKnots) ||
        !qFuzzyCompare(m_oldState.windageDirectionDegrees, newData.windageDirectionDegrees) ||
        m_oldState.windageAppliedToBallistics != newData.windageAppliedToBallistics) {
        windageParamsChanged = true;

        qDebug() << "[WeaponController] Windage parameters changed:"
                 << "Speed:" << m_oldState.windageSpeedKnots << "->" << newData.windageSpeedKnots << "knots"
                 << "Direction:" << m_oldState.windageDirectionDegrees << "->"
                 << newData.windageDirectionDegrees << "deg"
                 << "Applied:" << m_oldState.windageAppliedToBallistics << "->"
                 << newData.windageAppliedToBallistics;
    }

    // ========================================================================
    // ABSOLUTE BEARING CHANGE DETECTION (Critical for Windage!)
    // When absolute gimbal bearing changes, the crosswind component changes
    // even if wind conditions remain constant (crosswind = wind x sin(angle))
    // Absolute bearing = IMU yaw (vehicle heading) + Station azimuth
    // NOTE: azimuthDirection is uint16_t, use direct comparison not qFuzzyCompare
    // ========================================================================

    // Check station azimuth change
    if (m_oldState.azimuthDirection != newData.azimuthDirection) {
        if (newData.windageAppliedToBallistics && newData.windageSpeedKnots > 0.001f) {
            ballisticsInputsChanged = true;
        }
    }

    // Check IMU yaw change (vehicle rotation)
    if (!qFuzzyCompare(m_oldState.imuYawDeg, newData.imuYawDeg)) {
        if (newData.windageAppliedToBallistics && newData.windageSpeedKnots > 0.001f) {
            ballisticsInputsChanged = true;
        }
    }

    // ========================================================================
    // TRIGGER BALLISTICS RECALCULATION
    // Recalculate if LAC is active AND any input changed
    // OR if LAC was just activated/deactivated
    // OR if windage parameters changed
    // OR if absolute bearing changed with windage active
    // ========================================================================
    if (ballisticsInputsChanged || environmentalParamsChanged || windageParamsChanged) {
        updateFireControlSolution();
    }

    m_oldState = newData;
}

// ============================================================================
// AMMO FEED CYCLE FSM - OPERATOR ENTRY POINT
// ============================================================================
 
void WeaponController::setJamDetectionConfig(const JamDetectionConfig& config)
{
    m_jamConfig = config;
    m_jamMonitorTimer->setInterval(config.torqueSampleIntervalMs);
    
    qInfo() << "[WeaponController] Jam detection config updated";
    qInfo() << "  Nominal:" << config.torqueNominal;
    qInfo() << "  Jam threshold:" << config.torqueJamThreshold;
    qInfo() << "  Critical:" << config.torqueCritical;
}

// ============================================================================
// ACTUATOR FEEDBACK HANDLER
// ============================================================================

void WeaponController::onActuatorFeedback(const ServoActuatorData& data)
{
    // Update position (Ultramotion A2 reports in inches)
    float previousPosition = m_lastKnownPosition;
    m_lastKnownPosition = static_cast<float>(data.position_mm);  // Actually mm from A2!
    
    // Track position changes for stall detection
    if (std::abs(m_lastKnownPosition - previousPosition) > m_jamConfig.stallPositionTolerance) {
        m_lastPositionChangeTime = QDateTime::currentMSecsSinceEpoch();
    }

    // ========================================================================
    // TORQUE UPDATE (Critical for jam detection)
    // ServoActuatorDevice needs to be updated to capture TQ response
    // For now, we'll get torque from the merged data
    // ========================================================================
    // NOTE: You need to ensure ServoActuatorData has a field for raw torque
    // m_lastTorquePercent = data.torque_raw;  // Add this field!
    // Torque update - USE EXISTING FIELD!
    m_lastTorquePercent = data.torque_percent;

    if (m_lastTorquePercent > m_peakTorqueThisCycle) {
        m_peakTorqueThisCycle = m_lastTorquePercent;
    }
    
    // Emit for calibration UI
    emit torqueUpdated(m_lastTorquePercent);

    // ========================================================================
    // STATE-BASED POSITION PROCESSING
    // ========================================================================
    switch (m_feedState) {
    case AmmoFeedState::Extending:
        if (m_lastKnownPosition >= (FEED_EXTEND_POS - FEED_POSITION_TOLERANCE)) {
            // Extension complete
            stopJamMonitoring();
            m_feedTimer->stop();
            
            qInfo() << "[WeaponController] Extension COMPLETE at" << m_lastKnownPosition << "mm"
                    << "| Peak torque:" << m_peakTorqueThisCycle;
            
            if (m_stateModel && m_stateModel->data().ammoLoadButtonPressed) {
                transitionFeedState(AmmoFeedState::Extended);
            } else {
                // Short press - immediately retract
                transitionFeedState(AmmoFeedState::Retracting);
                commandRetract();
                startJamMonitoring();
                m_feedTimer->start(FEED_TIMEOUT_MS);
            }
        }
        break;

    case AmmoFeedState::Retracting:
        if (m_lastKnownPosition <= (FEED_RETRACT_POS + FEED_POSITION_TOLERANCE)) {
            // Retraction complete - cycle successful
            stopJamMonitoring();
            m_feedTimer->stop();
            transitionFeedState(AmmoFeedState::Idle);
            
            if (m_stateModel) {
                m_stateModel->setAmmoFeedCycleInProgress(false);
                m_stateModel->setAmmoLoaded(true);
            }
            
            qInfo() << "╔════════════════════════════════════════╗";
            qInfo() << "║   FEED CYCLE COMPLETED SUCCESSFULLY    ║";
            qInfo() << "╠════════════════════════════════════════╣";
            qInfo() << "║  Peak Torque:" << QString("%1").arg(m_peakTorqueThisCycle, 6) << "              ║";
            qInfo() << "║  Cycle Time: " << QString("%1").arg(m_cycleTimer.elapsed(), 5) << " ms            ║";
            qInfo() << "╚════════════════════════════════════════╝";
            
            emit ammoFeedCycleCompleted();
        }
        break;

    case AmmoFeedState::SafeRetract:
        if (m_lastKnownPosition <= (FEED_RETRACT_POS + FEED_POSITION_TOLERANCE)) {
            // Safe retract successful - but still enter fault for operator check
            handleSafeRetractComplete(true);
        }
        break;

    default:
        break;
    }
}

// ============================================================================
// JAM MONITORING (High-frequency timer callback)
// ============================================================================

void WeaponController::onJamMonitorTimer()
{
    processJamMonitoring();
}

void WeaponController::processJamMonitoring()
{
    // Only monitor during active motion states
    if (m_feedState != AmmoFeedState::Extending &&
        m_feedState != AmmoFeedState::Retracting &&
        m_feedState != AmmoFeedState::SafeRetract) {
        return;
    }

    // ========================================================================
    // CHANGED: Use torque_percent instead of raw value
    // No need for abs() - torque_percent is already absolute
    // ========================================================================
    float currentTorque = m_lastTorquePercent;

    // ========================================================================
    // CRITICAL TORQUE CHECK - Emergency stop (fuse protection)
    // ========================================================================
    if (currentTorque >= m_jamConfig.torqueCritical) {
        qCritical() << "╔════════════════════════════════════════╗";
        qCritical() << "║   ⚠️  CRITICAL TORQUE - FUSE PROTECT    ║";
        qCritical() << "╠════════════════════════════════════════╣";
        qCritical() << "║  Torque:" << QString("%1").arg(currentTorque, 6, 'f', 1) << "%"
                    << " >= " << QString("%1").arg(m_jamConfig.torqueCritical, 5, 'f', 1) << "%  ║";
        qCritical() << "╚════════════════════════════════════════╝";
        handleJamDetected("critical_torque");
        return;
    }

    // ========================================================================
    // JAM TORQUE CHECK - Definite jam condition
    // ========================================================================
    if (currentTorque >= m_jamConfig.torqueJamThreshold) {
        qWarning() << "[WeaponController] JAM TORQUE:" << currentTorque << "%"
                   << ">=" << m_jamConfig.torqueJamThreshold << "%";
        handleJamDetected("torque_jam");
        return;
    }

    // ========================================================================
    // WARNING TORQUE - Log but continue
    // ========================================================================
    if (currentTorque >= m_jamConfig.torqueWarning) {
        emit torqueWarning(currentTorque, m_jamConfig.torqueWarning);
    }

    // ========================================================================
    // STALL DETECTION - Position not changing
    // ========================================================================
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 timeSinceMove = now - m_lastPositionChangeTime;

    if (timeSinceMove > m_jamConfig.stallTimeMs) {
        // Check if we're at target (not stalled, just arrived)
        bool atTarget = false;

        if (m_feedState == AmmoFeedState::Extending) {
            atTarget = m_lastKnownPosition >= (FEED_EXTEND_POS - FEED_POSITION_TOLERANCE);
        } else if (m_feedState == AmmoFeedState::Retracting ||
                   m_feedState == AmmoFeedState::SafeRetract) {
            atTarget = m_lastKnownPosition <= (FEED_RETRACT_POS + FEED_POSITION_TOLERANCE);
        }

        if (!atTarget) {
            qWarning() << "[WeaponController] STALL DETECTED: No movement for"
                       << timeSinceMove << "ms at" << m_lastKnownPosition << "mm";
            handleJamDetected("stall");
            return;
        }
    }
}
// ============================================================================
// JAM HANDLING
// ============================================================================

void WeaponController::handleJamDetected(const QString& reason)
{
    // Prevent re-entry
    if (m_feedState == AmmoFeedState::JamDetected ||
        m_feedState == AmmoFeedState::SafeRetract ||
        m_feedState == AmmoFeedState::Fault) {
        return;
    }

    qCritical() << "╔════════════════════════════════════════════════════════════╗";
    qCritical() << "║                    JAM DETECTED                            ║";
    qCritical() << "╠════════════════════════════════════════════════════════════╣";
    qCritical() << "║  Reason:      " << QString("%1").arg(reason, -20) << "                   ║";
    qCritical() << "║  Position:    " << QString("%1").arg(m_lastKnownPosition, 6, 'f', 3) << " mm                          ║";
    qCritical() << "║  Torque:      " << QString("%1").arg(m_lastTorquePercent, 6) << " raw                         ║";
    qCritical() << "║  State:       " << QString("%1").arg(feedStateName(m_feedState), -15) << "                ║";
    qCritical() << "╚════════════════════════════════════════════════════════════╝";

    // IMMEDIATE STOP
    commandStop();
    stopJamMonitoring();
    m_feedTimer->stop();

    // Log the event
    JamEventData event;
    event.timestamp = QDateTime::currentMSecsSinceEpoch();
    event.stateWhenDetected = m_feedState;
    event.positionWhenDetected = m_lastKnownPosition;
    event.torquePercentWhenDetected = m_lastTorquePercent;  // was torqueRawWhenDetected
    event.detectionReason = reason;
    event.safeRetractSuccessful = false;

    m_jamHistory.append(event);
    while (m_jamHistory.size() > MAX_JAM_HISTORY) {
        m_jamHistory.removeFirst();
    }

    emit jamDetected(event);

    transitionFeedState(AmmoFeedState::JamDetected);

    // Start safe retract to home position
    startSafeRetract();
}

void WeaponController::startSafeRetract()
{
    qInfo() << "[WeaponController] Starting SAFE RETRACT to home position";
    
    transitionFeedState(AmmoFeedState::SafeRetract);
    emit safeRetractStarted();

    // Set reduced torque for safe retract
    setTorqueLimit(m_jamConfig.safeRetractTorqueLimit);

    // Command retract
    commandRetract();

    // Start timeout (if can't retract, go to fault anyway)
    m_safeRetractTimer->start(m_jamConfig.safeRetractTimeoutMs);
    
    // Continue monitoring (to detect if retract also jams)
    m_lastPositionChangeTime = QDateTime::currentMSecsSinceEpoch();
    m_jamMonitorTimer->start();
}

void WeaponController::onSafeRetractTimeout()
{
    qWarning() << "[WeaponController] Safe retract TIMEOUT - forcing fault state";
    handleSafeRetractComplete(false);
}

void WeaponController::handleSafeRetractComplete(bool success)
{
    m_safeRetractTimer->stop();
    stopJamMonitoring();
    commandStop();

    // Update last jam event
    if (!m_jamHistory.isEmpty()) {
        m_jamHistory.last().safeRetractSuccessful = success;
    }

    emit safeRetractCompleted(success);

    // Always go to FAULT state - operator must verify
    transitionFeedState(AmmoFeedState::Fault);

    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }

    QString faultReason = success 
        ? "JAM DETECTED - RETRACTED OK - CHECK GUN"
        : "JAM DETECTED - RETRACT FAILED - CHECK GUN";

    qCritical() << "╔════════════════════════════════════════════════════════════╗";
    qCritical() << "║                    FAULT STATE                             ║";
    qCritical() << "╠════════════════════════════════════════════════════════════╣";
    qCritical() << "║  Safe Retract: " << (success ? "SUCCESS" : "FAILED ") << "                            ║";
    qCritical() << "║  Position:     " << QString("%1").arg(m_lastKnownPosition, 6, 'f', 3) << " in                         ║";
    qCritical() << "║                                                            ║";
    qCritical() << "║  >>> OPERATOR MUST CHECK GUN AND PRESS LOAD TO RESET <<<   ║";
    qCritical() << "╚════════════════════════════════════════════════════════════╝";

    emit ammoFeedCycleFaulted(faultReason);
}


// ============================================================================
// JAM MONITORING CONTROL
// ============================================================================

void WeaponController::startJamMonitoring()
{
    m_lastPositionChangeTime = QDateTime::currentMSecsSinceEpoch();
    m_peakTorqueThisCycle = 0;
    m_jamMonitorTimer->start();
    qDebug() << "[WeaponController] Jam monitoring STARTED";
}

void WeaponController::stopJamMonitoring()
{
    m_jamMonitorTimer->stop();
    qDebug() << "[WeaponController] Jam monitoring STOPPED | Peak torque:" << m_peakTorqueThisCycle;
}


// ============================================================================
// MOTION COMMANDS
// ============================================================================

void WeaponController::commandExtend()
{
    if (!m_servoActuator) return;
    m_servoActuator->moveToPosition(FEED_EXTEND_POS);
    qDebug() << "[WeaponController] CMD: EXTEND to" << FEED_EXTEND_POS << "mm";
}

void WeaponController::commandRetract()
{
    if (!m_servoActuator) return;
    m_servoActuator->moveToPosition(FEED_RETRACT_POS);
    qDebug() << "[WeaponController] CMD: RETRACT to" << FEED_RETRACT_POS << "mm";
}

void WeaponController::commandStop()
{
    if (!m_servoActuator) return;
    m_servoActuator->stopMove();
    qDebug() << "[WeaponController] CMD: STOP";
}

void WeaponController::setTorqueLimit(int16_t rawLimit)
{
    if (!m_servoActuator) return;
    // Convert raw to percentage if needed, or send raw value
    // This depends on how your ServoActuatorDevice::setMaxTorque() works
    // For Ultramotion A2, you might need to use the MC (Max Current) command
    double percent = (static_cast<double>(rawLimit) / 32767.0) * 100.0;
    m_servoActuator->setMaxTorque(percent);
    qDebug() << "[WeaponController] CMD: TORQUE LIMIT" << rawLimit << "raw (" << percent << "%)";
}

// ============================================================================
// AMMO FEED CYCLE FSM - TIMEOUT HANDLER
// ============================================================================

void WeaponController::onOperatorRequestLoad()
{
    qDebug() << "[WeaponController] Operator REQUEST LOAD | State:" << feedStateName(m_feedState);

    // FAULT state - operator presses button to reset after checking gun
    if (m_feedState == AmmoFeedState::Fault) {
        resetFeedFault();
        return;
    }

    // Only start when idle
    if (m_feedState != AmmoFeedState::Idle) {
        qDebug() << "[WeaponController] Feed start IGNORED - not idle";
        return;
    }

    startAmmoFeedCycle();
}

void WeaponController::startAmmoFeedCycle()
{
    if (!m_servoActuator) {
        qWarning() << "[WeaponController] No servo actuator!";
        return;
    }

    qInfo() << "╔════════════════════════════════════════╗";
    qInfo() << "║        FEED CYCLE STARTED              ║";
    qInfo() << "╚════════════════════════════════════════╝";

    m_cycleTimer.start();
    m_peakTorqueThisCycle = 0;

    transitionFeedState(AmmoFeedState::Extending);

    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(true);
    }
    emit ammoFeedCycleStarted();

    commandExtend();
    startJamMonitoring();
    m_feedTimer->start(FEED_TIMEOUT_MS);
}

void WeaponController::resetFeedFault()
{
    if (m_feedState != AmmoFeedState::Fault) {
        qDebug() << "[WeaponController] Reset IGNORED - not in fault";
        return;
    }

    qInfo() << "[WeaponController] FAULT RESET by operator";
    qInfo() << "  (Operator has verified gun is clear)";

    transitionFeedState(AmmoFeedState::Idle);

    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }
}

void WeaponController::abortFeedCycle()
{
    qWarning() << "[WeaponController] ABORT by operator";

    commandStop();
    stopJamMonitoring();
    m_feedTimer->stop();
    m_safeRetractTimer->stop();

    transitionFeedState(AmmoFeedState::Idle);

    if (m_stateModel) {
        m_stateModel->setAmmoFeedCycleInProgress(false);
    }

    emit ammoFeedCycleAborted();
}

void WeaponController::onFeedTimeout()
{
    qWarning() << "[WeaponController] FEED TIMEOUT in state:" << feedStateName(m_feedState);
    handleJamDetected("timeout");
}

 
// ============================================================================
// STATE TRANSITION
// ============================================================================

void WeaponController::transitionFeedState(AmmoFeedState newState)
{
    qDebug() << "[WeaponController] State:" << feedStateName(m_feedState)
             << "->" << feedStateName(newState);
    m_feedState = newState;

    if (m_stateModel) {
        m_stateModel->setAmmoFeedState(newState);
    }
}

QString WeaponController::feedStateName(AmmoFeedState s) const
{
    switch (s) {
    case AmmoFeedState::Idle:        return "Idle";
    case AmmoFeedState::Extending:   return "Extending";
    case AmmoFeedState::Extended:    return "Extended";
    case AmmoFeedState::Retracting:  return "Retracting";
    case AmmoFeedState::JamDetected: return "JamDetected";
    case AmmoFeedState::SafeRetract: return "SafeRetract";
    case AmmoFeedState::Fault:       return "FAULT";
    }
    return "Unknown";
}

// ============================================================================
// WEAPON CONTROL
// ============================================================================

void WeaponController::armWeapon(bool enable)
{
    Q_UNUSED(enable)
    // Implementation placeholder
}

void WeaponController::fireSingleShot()
{
    // Implementation placeholder
}

void WeaponController::startFiring()
{
    if (!m_systemArmed) {
        qDebug() << "[WeaponController] Cannot fire: system is not armed";
        return;
    }

    // Safety: Prevent firing if current aim point is inside a No-Fire zone
    if (m_stateModel) {
        SystemStateData s = m_stateModel->data();
        if (m_stateModel->isPointInNoFireZone(s.gimbalAz, s.gimbalEl)) {
            qWarning() << "[WeaponController] FIRE BLOCKED: Aim point is inside a No-Fire zone. Solenoid NOT armed.";
            return;
        }

        // =====================================================================
        // CROWS DEAD RECKONING (TM 9-1090-225-10-2 page 38)
        // =====================================================================
        // "When firing is initiated, CROWS aborts Target Tracking. Instead the
        //  system moves according to the speed and direction of the WS just
        //  prior to pulling the trigger. CROWS will not automatically compensate
        //  for changes in speed or direction of the tracked target during firing."
        // =====================================================================
        if (s.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock ||
            s.currentTrackingPhase == TrackingPhase::Tracking_Coast) {
            qInfo() << "[CROWS] FIRING DURING TRACKING - Entering dead reckoning mode";
            m_stateModel->enterDeadReckoning(
                s.currentTargetAngularRateAz,
                s.currentTargetAngularRateEl
            );
        }
    }

    // All OK — send solenoid command
    m_plc42->setSolenoidState(1);
}

void WeaponController::stopFiring()
{
    m_plc42->setSolenoidState(0);

    // =========================================================================
    // CROWS: Exit dead reckoning when firing stops
    // =========================================================================
    // Per CROWS doctrine: Firing terminates tracking completely.
    // After firing, operator must re-acquire target to resume tracking.
    // =========================================================================
    if (m_stateModel) {
        SystemStateData s = m_stateModel->data();
        if (s.deadReckoningActive) {
            m_stateModel->exitDeadReckoning();
        }
    }
}

void WeaponController::unloadAmmo()
{
    // Legacy API - delegate to FSM if not in fault
    // For actual unload cycle, you might want a separate FSM or extend current one
    stopFiring();

    if (m_feedState == AmmoFeedState::Idle) {
        qDebug() << "[WeaponController] Unload requested - initiating retraction cycle";
        // Just retract (unload doesn't need extend)
        transitionFeedState(AmmoFeedState::Retracting);
        m_servoActuator->moveToPosition(FEED_RETRACT_POS);
        m_feedTimer->start(FEED_TIMEOUT_MS);

        if (m_stateModel) {
            m_stateModel->setAmmoFeedCycleInProgress(true);
        }
    } else {
        qDebug() << "[WeaponController] Cannot unload: feed cycle active or faulted";
    }
}

// ============================================================================
// FIRE CONTROL SOLUTION
// ============================================================================

void WeaponController::updateFireControlSolution()
{
    if (!m_stateModel) return;

    SystemStateData sData = m_stateModel->data();

    // ========================================================================
    // CALCULATE CROSSWIND FROM WINDAGE (Direction + Speed)
    // CRITICAL: This calculation runs INDEPENDENTLY of LAC status!
    // Windage provides absolute wind direction and speed (relative to true North).
    // Crosswind component varies with absolute gimbal bearing.
    // Absolute gimbal bearing = Vehicle heading (IMU yaw) + Station azimuth
    // ========================================================================
    float currentCrosswind = 0.0f;

    if (sData.windageAppliedToBallistics && sData.windageSpeedKnots > 0.001f) {
        // Convert wind speed from knots to m/s
        float windSpeedMS = sData.windageSpeedKnots * 0.514444f;  // 1 knot = 0.514444 m/s

        // Calculate absolute gimbal bearing (true bearing where weapon is pointing)
        float absoluteGimbalBearing = static_cast<float>(sData.imuYawDeg) + sData.azimuthDirection;

        // Normalize to 0-360 range
        while (absoluteGimbalBearing >= 360.0f) absoluteGimbalBearing -= 360.0f;
        while (absoluteGimbalBearing < 0.0f) absoluteGimbalBearing += 360.0f;

        // Calculate crosswind component
        currentCrosswind = calculateCrosswindComponent(
            windSpeedMS,
            sData.windageDirectionDegrees,
            absoluteGimbalBearing
        );

        // Store calculated crosswind in state data for OSD display
        if (std::abs(sData.calculatedCrosswindMS - currentCrosswind) > 0.01f) {
            SystemStateData updatedData = sData;
            updatedData.calculatedCrosswindMS = currentCrosswind;
            m_stateModel->updateData(updatedData);
            sData = updatedData;
        }
    } else {
        // Clear crosswind when windage is not applied
        if (std::abs(sData.calculatedCrosswindMS) > 0.01f) {
            SystemStateData updatedData = sData;
            updatedData.calculatedCrosswindMS = 0.0f;
            m_stateModel->updateData(updatedData);
            sData = updatedData;
        }
    }

    // ========================================================================
    // PROFESSIONAL FCS: SPLIT BALLISTIC DROP AND MOTION LEAD
    // Drop compensation: Auto-applied when LRF range valid (Kongsberg/Rafael)
    // Motion lead: Only when LAC toggle active
    // ========================================================================
    if (!m_ballisticsProcessor) {
        qCritical() << "[WeaponController] BallisticsProcessor pointer is NULL!";
        return;
    }

    float targetRange = sData.currentTargetRange;
    float currentHFOV = sData.activeCameraIsDay ? sData.dayCurrentHFOV : sData.nightCurrentHFOV;
    float currentVFOV = sData.activeCameraIsDay ? sData.dayCurrentVFOV : sData.nightCurrentVFOV;

  // ========================================================================
    // STEP 1: UPDATE ENVIRONMENTAL CONDITIONS (affects both drop and lead)
    // ========================================================================
    if (sData.environmentalAppliedToBallistics) {
        m_ballisticsProcessor->setEnvironmentalConditions(
            sData.environmentalTemperatureCelsius,
            sData.environmentalAltitudeMeters,
            currentCrosswind
        );
    } else {
        m_ballisticsProcessor->setEnvironmentalConditions(
            15.0f, 0.0f, currentCrosswind  // Standard conditions + wind
        );
    }

    // ========================================================================
    // STEP 2: BALLISTIC DROP - AUTO-APPLIED WHEN RANGE VALID (Professional FCS)
    // ========================================================================
    // DEFAULT_LAC_RANGE: Used for motion lead calculation when LRF is cleared
    // but LAC is active. Professional FCS typically uses 500-1000m default.
    // This allows CCIP to function for moving targets even without LRF lock.
    // ========================================================================
    constexpr float DEFAULT_LAC_RANGE = 500.0f;  // meters

    bool applyDrop = (targetRange > 0.1f);  // Valid range threshold

    if (applyDrop) {
        LeadCalculationResult drop = m_ballisticsProcessor->calculateBallisticDrop(targetRange);

        SystemStateData updatedData = sData;
        updatedData.ballisticDropOffsetAz = drop.leadAzimuthDegrees;
        updatedData.ballisticDropOffsetEl = drop.leadElevationDegrees;
        updatedData.ballisticDropActive = true;
        m_stateModel->updateData(updatedData);
        sData = updatedData;

        /*qDebug() << "[WeaponController] AUTO DROP APPLIED:"
                 << "Range:" << targetRange << "m"
                 << "| Drop Az:" << drop.leadAzimuthDegrees << "deg (wind)"
                 << "| Drop El:" << drop.leadElevationDegrees << "deg (gravity)";*/
    } else {
        // Clear drop when no valid range
        if (sData.ballisticDropActive) {
            SystemStateData updatedData = sData;
            updatedData.ballisticDropOffsetAz = 0.0f;
            updatedData.ballisticDropOffsetEl = 0.0f;
            updatedData.ballisticDropActive = false;
            m_stateModel->updateData(updatedData);
            sData = updatedData;

            qDebug() << "[WeaponController] DROP CLEARED (no valid range)";
        }
    }

    // ========================================================================
    // STEP 3: MOTION LEAD - ONLY WHEN LAC TOGGLE ACTIVE (Professional FCS)
    // ========================================================================
    if (!sData.leadAngleCompensationActive) {
        // LAC toggle is OFF - clear motion lead
        if (sData.motionLeadOffsetAz != 0.0f || sData.motionLeadOffsetEl != 0.0f ||
            sData.currentLeadAngleStatus != LeadAngleStatus::Off) {

            SystemStateData updatedData = sData;
            updatedData.motionLeadOffsetAz = 0.0f;
            updatedData.motionLeadOffsetEl = 0.0f;
            updatedData.currentLeadAngleStatus = LeadAngleStatus::Off;
            // Backward compatibility
            updatedData.leadAngleOffsetAz = 0.0f;
            updatedData.leadAngleOffsetEl = 0.0f;
            m_stateModel->updateData(updatedData);

            qDebug() << "[WeaponController] MOTION LEAD CLEARED (LAC off)";
        }
        return;  // Exit - drop already applied above if range valid
    }

    // LAC is ACTIVE - calculate motion lead for moving target
    float targetAngRateAz = sData.currentTargetAngularRateAz;
    float targetAngRateEl = sData.currentTargetAngularRateEl;

    // ========================================================================
    // BUG FIX: USE DEFAULT RANGE FOR MOTION LEAD WHEN LRF IS CLEARED
    // ========================================================================
    // Motion lead requires TOF which depends on range. When LRF is cleared:
    // - Use DEFAULT_LAC_RANGE (500m) for TOF calculation
    // - This allows CCIP to work for close-range moving targets
    // - Status will show "Lag" to indicate estimated range is used
    // ========================================================================
    float leadCalculationRange = (targetRange > 0.1f) ? targetRange : DEFAULT_LAC_RANGE;

    LeadCalculationResult lead = m_ballisticsProcessor->calculateMotionLead(
        leadCalculationRange,
        targetAngRateAz,
        targetAngRateEl,
        currentHFOV,
        currentVFOV
    );

    // If using default range (no LRF), force Lag status to indicate estimation
    if (targetRange <= 0.1f && lead.status == LeadAngleStatus::On) {
        lead.status = LeadAngleStatus::Lag;
    }

    SystemStateData updatedData = sData;
    updatedData.motionLeadOffsetAz = lead.leadAzimuthDegrees;
    updatedData.motionLeadOffsetEl = lead.leadElevationDegrees;
    updatedData.currentLeadAngleStatus = lead.status;
    // Backward compatibility
    updatedData.leadAngleOffsetAz = lead.leadAzimuthDegrees;
    updatedData.leadAngleOffsetEl = lead.leadElevationDegrees;
    m_stateModel->updateData(updatedData);

    qDebug() << "[WeaponController] MOTION LEAD:"
             << "Az:" << lead.leadAzimuthDegrees << "deg"
             << "| El:" << lead.leadElevationDegrees << "deg"
             << "| Range:" << leadCalculationRange << "m"
             << (targetRange <= 0.1f ? "(DEFAULT)" : "(LRF)")
             << "| Status:" << static_cast<int>(lead.status)
             << (lead.status == LeadAngleStatus::On ? "(On)" :
                 lead.status == LeadAngleStatus::Lag ? "(Lag)" :
                 lead.status == LeadAngleStatus::ZoomOut ? "(ZoomOut)" : "(Unknown)");
}

// ============================================================================
// CROSSWIND CALCULATION
// ============================================================================

float WeaponController::calculateCrosswindComponent(float windSpeedMS,
                                                    float windDirectionDeg,
                                                    float gimbalAzimuthDeg)
{
    // ========================================================================
    // BALLISTICS PHYSICS: Crosswind Component Calculation
    // Wind direction: Direction wind is coming FROM (meteorological convention)
    // Gimbal azimuth: Direction weapon is pointing TO
    //
    // Relative angle = wind_direction - firing_direction
    // Crosswind = wind_speed x sin(relative_angle)
    //
    // Examples:
    //   Wind from 0deg (North), firing at 0deg (North):
    //     relative = 0deg -> sin(0deg) = 0 -> no crosswind (headwind)
    //   Wind from 0deg (North), firing at 90deg (East):
    //     relative = -90deg -> sin(-90deg) = -1 -> full crosswind (left)
    //   Wind from 0deg (North), firing at 180deg (South):
    //     relative = -180deg -> sin(-180deg) = 0 -> no crosswind (tailwind)
    //   Wind from 0deg (North), firing at 270deg (West):
    //     relative = -270deg = +90deg -> sin(90deg) = +1 -> full crosswind (right)
    // ========================================================================

    float relativeAngle = windDirectionDeg - gimbalAzimuthDeg;

    // Normalize to [-180, +180] for correct trigonometry
    while (relativeAngle > 180.0f) {
        relativeAngle -= 360.0f;
    }
    while (relativeAngle < -180.0f) {
        relativeAngle += 360.0f;
    }

    // Calculate crosswind component
    // sin(90deg) = 1.0 -> full crosswind (wind perpendicular to fire direction)
    // sin(0deg) = 0.0 -> no crosswind (headwind or tailwind)
    float crosswindMS = windSpeedMS * std::sin(relativeAngle * static_cast<float>(M_PI) / 180.0f);

    return crosswindMS;
}

// ============================================================================
// INCLUDES
// ============================================================================

#include "gimbalcontroller.h"

// Motion Modes
#include "motion_modes/manualmotionmode.h"
#include "motion_modes/trackingmotionmode.h"
#include "motion_modes/autosectorscanmotionmode.h"
#include "motion_modes/radarslewmotionmode.h"
#include "motion_modes/trpscanmotionmode.h"

// Hardware Devices
#include "hardware/devices/servodriverdevice.h"
#include "hardware/devices/plc42device.h"

// Safety
#include "safety/SafetyInterlock.h"

// Qt
#include <QDebug>

// Standard Library
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cmath>

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

namespace GimbalUtils {

/**
 * @brief Convert pixel error to angular offset in degrees
 * @param errorPxX Horizontal pixel error (tracked_pos - screen_center)
 * @param errorPxY Vertical pixel error (tracked_pos - screen_center)
 * @param imageWidthPx Image width in pixels
 * @param imageHeightPx Image height in pixels
 * @param cameraHfovDegrees Camera horizontal field of view in degrees
 * @return QPointF(azimuth_offset_deg, elevation_offset_deg)
 *
 * Sign convention:
 * - Positive azimuth offset = gimbal should move RIGHT
 * - Positive elevation offset = gimbal should move UP
 * - Positive Y pixel error = target is BELOW center (needs DOWN correction)
 */
QPointF calculateAngularOffsetFromPixelError(double errorPxX, double errorPxY, int imageWidthPx,
                                             int imageHeightPx, float cameraHfovDegrees) {
    double angularOffsetXDeg = 0.0;
    double angularOffsetYDeg = 0.0;

    // Calculate azimuth offset
    if (cameraHfovDegrees > 0.01f && imageWidthPx > 0) {
        double degreesPerPixelAz = cameraHfovDegrees / static_cast<double>(imageWidthPx);
        angularOffsetXDeg = errorPxX * degreesPerPixelAz;
    }

    // Calculate elevation offset
    if (cameraHfovDegrees > 0.01f && imageWidthPx > 0 && imageHeightPx > 0) {
        double aspectRatio = static_cast<double>(imageWidthPx) / static_cast<double>(imageHeightPx);
        double vfov_rad_approx =
            2.0 * std::atan(std::tan((cameraHfovDegrees * M_PI / 180.0) / 2.0) / aspectRatio);
        double vfov_deg_approx = vfov_rad_approx * 180.0 / M_PI;

        if (vfov_deg_approx > 0.01f) {
            double degreesPerPixelEl = vfov_deg_approx / static_cast<double>(imageHeightPx);
            angularOffsetYDeg = -errorPxY * degreesPerPixelEl;
        }
    }

    return QPointF(angularOffsetXDeg, angularOffsetYDeg);
}

}  // namespace GimbalUtils

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

GimbalController::GimbalController(ServoDriverDevice* azServo, ServoDriverDevice* elServo,
                                   Plc42Device* plc42, SystemStateModel* stateModel,
                                   SafetyInterlock* safetyInterlock, QObject* parent)
    : QObject(parent), m_azServo(azServo), m_elServo(elServo), m_plc42(plc42),
      m_stateModel(stateModel), m_safetyInterlock(safetyInterlock) {
    // Log SafetyInterlock status
    if (m_safetyInterlock) {
        qInfo()
            << "[GimbalController] SafetyInterlock connected - motion safety via central authority";
    } else {
        qWarning()
            << "[GimbalController] WARNING: SafetyInterlock is null - using legacy safety checks";
    }
    // Initialize default motion mode
    setMotionMode(MotionMode::Idle);

    // Connect to system state changes
    // Direct connection (Qt::AutoConnection) - all components in main thread due to QModbus
    // Previous Qt::QueuedConnection was causing event queue saturation and latency issues
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged, this,
                &GimbalController::onSystemStateChanged);
    }

    // Connect alarm signals
    connect(m_azServo, &ServoDriverDevice::alarmDetected, this,
            &GimbalController::onAzAlarmDetected);
    connect(m_azServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onAzAlarmCleared);
    connect(m_elServo, &ServoDriverDevice::alarmDetected, this,
            &GimbalController::onElAlarmDetected);
    connect(m_elServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onElAlarmCleared);

    // Start periodic update timer (20Hz)
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &GimbalController::update);
    m_updateTimer->start(50);

    // Initialize centralized dt measurement timer (Expert Review Fix)
    m_velocityTimer.start();

    // Initialize HomingController (extracted FSM)
    m_homingController = new HomingController(m_plc42, this);
    connect(m_homingController, &HomingController::homingCompleted, this,
            &GimbalController::onHomingCompleted);
    connect(m_homingController, &HomingController::homingFailed, this,
            &GimbalController::onHomingFailed);
    connect(m_homingController, &HomingController::homingAborted, this,
            &GimbalController::onHomingAborted);

    qDebug() << "[GimbalController] Initialized with HomingController";
}

GimbalController::~GimbalController() {
    shutdown();
}

// ============================================================================
// SHUTDOWN
// ============================================================================

void GimbalController::shutdown() {
    // Stop the update timer FIRST to prevent any more updates
    if (m_updateTimer) {
        m_updateTimer->stop();
    }

    // CRITICAL: Clear state model pointer BEFORE exitMode()
    // SystemStateModel may already be destroyed when this destructor runs
    // (Qt destroys children in reverse creation order, but SystemStateModel
    // is a sibling created before ControllerRegistry in SystemController)
    m_stateModel = nullptr;

    // Exit current motion mode
    // The null check in stopServos/sendStabilizedServoCommands will handle this safely
    if (m_currentMode) {
        m_currentMode->exitMode(this);
    }
}

// ============================================================================
// UPDATE LOOP
// ============================================================================

void GimbalController::update() {
    // ============================================================================
    // Shutdown Safety Check
    // Timer may fire after shutdown() stops it if event was already queued
    // ============================================================================
    if (!m_stateModel) {
        return;  // SystemStateModel destroyed, skip update
    }

    // ============================================================================
    // Startup Sanity Checks (run once on first update)
    // ============================================================================
    static bool sanityChecksPerformed = false;
    if (!sanityChecksPerformed) {
        SystemStateData state = m_stateModel->data();

        // Verify IMU gyro rates are in deg/s (not rad/s)
        // Expected: typical vehicle rotation rates are 0-100 deg/s
        // If units were rad/s, typical values would be 0-1.75 rad/s
        const double MAX_REASONABLE_GYRO_DEGS = 500.0;  // deg/s (aggressive maneuver)
        const double MAX_REASONABLE_GYRO_RADS = 10.0;   // rad/s (~573 deg/s, clearly wrong)

        if (state.imuConnected) {
            double gyroMag = std::sqrt(state.GyroX * state.GyroX + state.GyroY * state.GyroY +
                                       state.GyroZ * state.GyroZ);

            if (gyroMag > MAX_REASONABLE_GYRO_DEGS) {
                qWarning() << "[GimbalController] IMU gyro rates suspiciously high:" << "mag ="
                           << gyroMag << "deg/s" << "- Check units (should be deg/s, not rad/s)";
            } else if (gyroMag > 0.001 && gyroMag < MAX_REASONABLE_GYRO_RADS) {
                qWarning() << "[GimbalController] IMU gyro rates suspiciously low:" << "mag ="
                           << gyroMag << "- Check units (should be deg/s, not rad/s)";
            }
        }

        // Verify stabilizer limits relationships
        const auto& stabCfg = MotionTuningConfig::instance().stabilizer;
        double componentSum = stabCfg.maxPositionVel + stabCfg.maxVelocityCorr;
        if (stabCfg.maxTotalVel < componentSum) {
            qWarning() << "[GimbalController] Stabilizer limit mismatch:" << "maxTotalVel ("
                       << stabCfg.maxTotalVel << ") < " << "maxPositionVel + maxVelocityCorr ("
                       << componentSum << ")" << "- Position correction will saturate early";
        }

        sanityChecksPerformed = true;
        qDebug() << "[GimbalController] Startup sanity checks completed";
    }

    // ============================================================================
    // Centralized dt Computation (Expert Review Fix)
    // ============================================================================
    double dt = m_velocityTimer.restart() / 1000.0;  // Convert ms to seconds
    dt = std::clamp(dt, 0.001, 0.050);               // Clamp between 1-50 ms

    // ============================================================================
    // MANUAL MODE LAC SUPPORT (CROWS-like behavior)
    // ============================================================================
    // Per CROWS TM 9-1090-225-10-2, page 2-26:
    // "The computer is constantly monitoring the change in rotation of the
    //  elevation and azimuth axes and measuring the speed."
    //
    // In Manual mode, the operator manually tracks a moving target with the
    // joystick. The gimbal's angular velocity IS the target's apparent angular
    // velocity (since the operator is compensating for target motion).
    //
    // This allows LAC to work in BOTH modes:
    // - AutoTrack: Uses tracker-measured target angular velocity
    // - Manual:    Uses gimbal angular velocity (operator tracking speed)
    // ============================================================================
    SystemStateData currentState = m_stateModel->data();

    if (m_gimbalVelocityInitialized && dt > 0.001) {
        // Calculate gimbal angular velocity from position derivative
        double gimbalAngularVelAz = (currentState.gimbalAz - m_prevGimbalAz) / dt;
        double gimbalAngularVelEl = (currentState.gimbalEl - m_prevGimbalEl) / dt;

        // Low-pass filter to smooth out measurement noise (tau = 100ms)
        static double filteredGimbalVelAz = 0.0;
        static double filteredGimbalVelEl = 0.0;
        const double filterTau = 0.1;  // 100ms time constant
        double alpha = dt / (filterTau + dt);
        filteredGimbalVelAz = alpha * gimbalAngularVelAz + (1.0 - alpha) * filteredGimbalVelAz;
        filteredGimbalVelEl = alpha * gimbalAngularVelEl + (1.0 - alpha) * filteredGimbalVelEl;

        // Apply Manual mode LAC when:
        // 1. In Manual motion mode (not AutoTrack)
        // 2. LAC toggle is active
        // 3. Not in any scan modes or other automated modes
        if ((currentState.motionMode == MotionMode::Manual ||
             currentState.motionMode == MotionMode::AutoTrack) &&
            currentState.leadAngleCompensationActive) {
            // Use gimbal angular velocity as "target angular rate" for LAC
            // This matches CROWS behavior where operator tracking speed is used
            m_stateModel->updateTargetAngularRates(static_cast<float>(filteredGimbalVelAz),
                                                   static_cast<float>(filteredGimbalVelEl));

            // Debug output (throttled)
            static int manualLacLogCounter = 0;
            if (++manualLacLogCounter % 40 == 0) {  // Every 2 seconds @ 20Hz
                qDebug() << "[GimbalController] MANUAL MODE LAC:" << "Gimbal velocity Az:"
                         << filteredGimbalVelAz << "°/s" << "El:" << filteredGimbalVelEl << "°/s";
            }
        }
    }

    // Store current position for next cycle's velocity calculation
    m_prevGimbalAz = currentState.gimbalAz;
    m_prevGimbalEl = currentState.gimbalEl;
    m_gimbalVelocityInitialized = true;

    // ============================================================================
    // Update Loop Timing Diagnostics
    // ============================================================================
    static auto lastUpdateTime = std::chrono::high_resolution_clock::now();
    static std::vector<long long> updateIntervals;
    static int updateCount = 0;

    auto now = std::chrono::high_resolution_clock::now();
    auto interval =
        std::chrono::duration_cast<std::chrono::microseconds>(now - lastUpdateTime).count();

    updateIntervals.push_back(interval);
    if (updateIntervals.size() > 100) {
        updateIntervals.erase(updateIntervals.begin());
    }

    if (++updateCount % 50 == 0) {
        long long minInterval = *std::min_element(updateIntervals.begin(), updateIntervals.end());
        long long maxInterval = *std::max_element(updateIntervals.begin(), updateIntervals.end());
        long long avgInterval =
            std::accumulate(updateIntervals.begin(), updateIntervals.end(), 0LL) /
            static_cast<long long>(updateIntervals.size());
        double jitter = (maxInterval - minInterval) / 1000.0;

        /*qDebug() << "[UPDATE LOOP] 50 cycles |"
                 << "Target: 50.0ms |"
                 << "Actual avg:" << (avgInterval / 1000.0) << "ms |"
                 << "Min:" << (minInterval / 1000.0) << "ms |"
                 << "Max:" << (maxInterval / 1000.0) << "ms |"
                 << "Jitter:" << jitter << "ms"
                 << (jitter > 10 ? "[HIGH]" : "[OK]");*/
    }

    lastUpdateTime = now;

    // ============================================================================
    // Motion Mode Update
    // ============================================================================
    if (!m_currentMode) {
        return;
    }

    // Update gyro bias based on latest stationary status
    m_currentMode->updateGyroBias(m_stateModel->data());

    // Execute motion mode update if safety conditions are met
    // Pass centralized dt to motion mode (Expert Review Fix)
    if (m_currentMode->checkSafetyConditions(this)) {
        m_currentMode->update(this, dt);
    } else {
        // Stop servos if safety conditions fail
        m_currentMode->stopServos(this);
    }
}

// ============================================================================
// STATE CHANGE HANDLER
// ============================================================================

void GimbalController::onSystemStateChanged(const SystemStateData& newData) {
    // ============================================================================
    // Performance Optimization: Early exit checks
    // Only process expensive operations if relevant state has changed
    // This prevents wasting CPU on servo position updates (110 Hz!)
    // ============================================================================

    // PRIORITY 1: EMERGENCY STOP (highest priority!)
    if (newData.emergencyStopActive != m_oldState.emergencyStopActive) {
        processEmergencyStop(newData);
    }

    // If emergency stop active, skip all other processing
    if (newData.emergencyStopActive) {
        m_oldState = newData;
        return;
    }

    // PRIORITY 2: HOMING SEQUENCE (delegated to HomingController)
    // ⭐ BUG FIX: Also check HOME-END signal changes (azimuthHomeComplete, elevationHomeComplete)
    // Without this, HomingController never receives HOME-END and times out
    if (newData.homingState != m_oldState.homingState ||
        newData.gotoHomePosition != m_oldState.gotoHomePosition ||
        newData.azimuthHomeComplete != m_oldState.azimuthHomeComplete ||
        newData.elevationHomeComplete != m_oldState.elevationHomeComplete) {
        m_homingController->process(newData, m_oldState);
    }

    // If homing in progress, skip motion mode changes to avoid interference
    if (m_homingController->isHomingInProgress()) {
        m_oldState = newData;
        return;
    }

    // PRIORITY 3: FREE MODE MONITORING
    if (newData.freeGimbalState != m_oldState.freeGimbalState) {
        processFreeMode(newData);
    }

    // ============================================================================
    // Motion Mode Change Detection
    // ============================================================================
    bool motionModeTypeChanged = (m_oldState.motionMode != newData.motionMode);
    bool scanParametersChanged = false;

    // Check for scan parameter changes (only if mode type unchanged)
    if (!motionModeTypeChanged) {
        if (newData.motionMode == MotionMode::AutoSectorScan &&
            m_oldState.activeAutoSectorScanZoneId != newData.activeAutoSectorScanZoneId) {
            qDebug() << "[GimbalController] AutoSectorScanZone changed to"
                     << newData.activeAutoSectorScanZoneId;
            scanParametersChanged = true;
        } else if (newData.motionMode == MotionMode::TRPScan &&
                   m_oldState.activeTRPLocationPage != newData.activeTRPLocationPage) {
            qDebug() << "[GimbalController] TRPLocationPage changed to"
                     << newData.activeTRPLocationPage;
            scanParametersChanged = true;
        }
    }

    // ============================================================================
    // Tracking Target Updates (Performance: only when tracking data changed)
    // ============================================================================
    if (newData.motionMode == MotionMode::AutoTrack && m_currentMode) {
        if (dynamic_cast<TrackingMotionMode*>(m_currentMode.get())) {
            bool trackingDataChanged =
                (newData.trackerHasValidTarget != m_oldState.trackerHasValidTarget ||
                 newData.trackedTargetCenterX_px != m_oldState.trackedTargetCenterX_px ||
                 newData.trackedTargetCenterY_px != m_oldState.trackedTargetCenterY_px ||
                 newData.trackedTargetVelocityX_px_s != m_oldState.trackedTargetVelocityX_px_s ||
                 newData.trackedTargetVelocityY_px_s != m_oldState.trackedTargetVelocityY_px_s);

            if (trackingDataChanged) {
                if (newData.trackerHasValidTarget) {
                    float screenCenterX_px = newData.currentImageWidthPx / 2.0f;
                    float screenCenterY_px = newData.currentImageHeightPx / 2.0f;

                    double errorPxX = newData.trackedTargetCenterX_px - screenCenterX_px;
                    double errorPxY = newData.trackedTargetCenterY_px - screenCenterY_px;

                    // Get active camera FOV
                    float activeHfov = newData.activeCameraIsDay
                                           ? static_cast<float>(newData.dayCurrentHFOV)
                                           : static_cast<float>(newData.nightCurrentHFOV);

                    QPointF angularOffset = GimbalUtils::calculateAngularOffsetFromPixelError(
                        errorPxX, errorPxY, newData.currentImageWidthPx,
                        newData.currentImageHeightPx, activeHfov);

                    errorPxX = newData.trackedTargetCenterX_px - screenCenterX_px;
                    errorPxY = newData.trackedTargetCenterY_px - screenCenterY_px;

                    // Convert pixel error → angular error (deg)
                    QPointF angularError = GimbalUtils::calculateAngularOffsetFromPixelError(
                        errorPxX, errorPxY, newData.currentImageWidthPx,
                        newData.currentImageHeightPx, activeHfov);

                    // Convert pixel velocity → angular velocity (deg/s)
                    QPointF angularVelocity = GimbalUtils::calculateAngularOffsetFromPixelError(
                        newData.trackedTargetVelocityX_px_s, newData.trackedTargetVelocityY_px_s,
                        newData.currentImageWidthPx, newData.currentImageHeightPx, activeHfov);

                    // ✔ EMIT IMAGE-SPACE DATA ONLY
                    emit trackingTargetUpdated(
                        angularError.x(),  // image-based angular error (deg)
                        angularError.y(),
                        angularVelocity.x(),  // target angular velocity (deg/s)
                        angularVelocity.y(), true);
                } else {
                    // Target lost - clear angular rates and emit invalid signal
                    m_stateModel->updateTargetAngularRates(0.0f, 0.0f);
                    emit trackingTargetUpdated(0, 0, 0, 0, false);
                }
            }
        }
    } else if (m_oldState.motionMode == MotionMode::AutoTrack &&
               newData.motionMode != MotionMode::AutoTrack) {
        // ============================================================================
        // BUG FIX #1: Clear angular rates when exiting tracking mode
        // ============================================================================
        // Ensures rates are zeroed when tracking stops, so LAC doesn't
        // continue using stale velocity data.
        // ============================================================================
        m_stateModel->updateTargetAngularRates(0.0f, 0.0f);
    }

    // ============================================================================
    // MANUAL MODE LAC: Clear angular rates when exiting Manual mode or
    // when LAC is deactivated while in Manual mode
    // ============================================================================
    bool wasManualWithLAC =
        (m_oldState.motionMode == MotionMode::Manual && m_oldState.leadAngleCompensationActive);
    bool isManualWithLAC =
        (newData.motionMode == MotionMode::Manual && newData.leadAngleCompensationActive);

    if (wasManualWithLAC && !isManualWithLAC) {
        // Exited Manual mode or LAC was deactivated - clear angular rates
        m_stateModel->updateTargetAngularRates(0.0f, 0.0f);
        qDebug() << "[GimbalController] Manual mode LAC ended - angular rates cleared";
    }

    // Reconfigure motion mode if type or parameters changed
    if (motionModeTypeChanged || scanParametersChanged) {
        setMotionMode(newData.motionMode);
    }

    // Update no-traverse zone status
    bool inNTZ = m_stateModel->isPointInNoTraverseZone(newData.gimbalAz, newData.gimbalEl);
    if (newData.isReticleInNoTraverseZone != inNTZ) {
        m_stateModel->setPointInNoTraverseZone(inNTZ);
    }
    bool inNFZ = m_stateModel->isPointInNoFireZone(newData.gimbalAz, newData.gimbalEl);
    if (newData.isReticleInNoFireZone != inNFZ) {
        m_stateModel->setPointInNoFireZone(inNFZ);
    }
    m_oldState = newData;
}

// ============================================================================
// MOTION MODE MANAGEMENT
// ============================================================================

void GimbalController::setMotionMode(MotionMode newMode) {
    // Exit old mode
    if (m_currentMode) {
        m_currentMode->exitMode(this);
    }

    // Create the corresponding motion mode class
    switch (newMode) {
    case MotionMode::Manual:
        m_currentMode = std::make_unique<ManualMotionMode>();
        break;

    case MotionMode::AutoTrack:
    case MotionMode::ManualTrack: {
        auto trackingMode = std::make_unique<TrackingMotionMode>();
        // Direct connection - motion mode runs in same thread as GimbalController
        connect(this, &GimbalController::trackingTargetUpdated, trackingMode.get(),
                &TrackingMotionMode::onTargetPositionUpdated);
        m_currentMode = std::move(trackingMode);
        break;
    }

    case MotionMode::RadarSlew:
        m_currentMode = std::make_unique<RadarSlewMotionMode>();
        break;

    case MotionMode::AutoSectorScan: {
        auto scanMode = std::make_unique<AutoSectorScanMotionMode>();
        const auto& scanZones = m_stateModel->data().sectorScanZones;
        int activeId = m_stateModel->data().activeAutoSectorScanZoneId;

        auto it = std::find_if(
            scanZones.begin(), scanZones.end(),
            [activeId](const AutoSectorScanZone& z) { return z.id == activeId && z.isEnabled; });

        if (it != scanZones.end()) {
            scanMode->setActiveScanZone(*it);
            m_currentMode = std::move(scanMode);
        } else {
            qWarning() << "[GimbalController] AutoSectorScan zone" << activeId
                       << "not found or disabled";
            m_currentMode = nullptr;
            newMode = MotionMode::Idle;
        }
        break;
    }

    case MotionMode::TRPScan: {
        auto trpMode = std::make_unique<TRPScanMotionMode>();
        const auto& allTrps = m_stateModel->data().targetReferencePoints;
        int activePageNum = m_stateModel->data().activeTRPLocationPage;

        std::vector<TargetReferencePoint> pageToScan;
        for (const auto& trp : allTrps) {
            if (trp.locationPage == activePageNum) {
                pageToScan.push_back(trp);
            }
        }

        if (!pageToScan.empty()) {
            trpMode->setTrpList(pageToScan);
            m_currentMode = std::move(trpMode);
        } else {
            qWarning() << "[GimbalController] No TRPs for page" << activePageNum;
            m_currentMode = nullptr;
            newMode = MotionMode::Idle;
        }
        break;
    }

    case MotionMode::MotionFree:
        // Free mode - no active control, brakes released
        // This mode is activated by the physical FREE toggle switch at station
        // No motion mode object needed - servos are held in FREE by PLC42
        m_currentMode = nullptr;
        qInfo() << "[GimbalController] Motion mode set to FREE (no active control)";
        break;

    default:
        qWarning() << "[GimbalController] Unknown motion mode" << static_cast<int>(newMode);
        m_currentMode = nullptr;
        break;
    }

    m_currentMotionModeType = newMode;

    // Enter new mode
    if (m_currentMode) {
        m_currentMode->enterMode(this);
    }

    qDebug() << "[GimbalController] Motion mode set to"
             << static_cast<int>(m_currentMotionModeType);
}

// ============================================================================
// ALARM MANAGEMENT
// ============================================================================

void GimbalController::readAlarms() {
    if (m_azServo) {
        m_azServo->readAlarmStatus();
    }
    if (m_elServo) {
        m_elServo->readAlarmStatus();
    }
}

void GimbalController::clearAlarms() {
    // Clear PLC42 alarm state with two-step reset sequence
    m_plc42->setResetAlarm(0);

    // Second reset command after 1 second delay
    QTimer::singleShot(1000, this, [this]() { m_plc42->setResetAlarm(1); });
}

void GimbalController::onAzAlarmDetected(uint16_t alarmCode, const QString& description) {
    emit azAlarmDetected(alarmCode, description);
}

void GimbalController::onAzAlarmCleared() {
    emit azAlarmCleared();
}

void GimbalController::onElAlarmDetected(uint16_t alarmCode, const QString& description) {
    emit elAlarmDetected(alarmCode, description);
}

void GimbalController::onElAlarmCleared() {
    emit elAlarmCleared();
}

// ============================================================================
// HOMINGCONTROLLER HANDLERS
// ============================================================================
// NOTE: Homing FSM has been extracted to HomingController class for testability.
// These handlers respond to HomingController signals.
// ============================================================================

void GimbalController::onHomingCompleted() {
    qInfo() << "[GimbalController] Homing completed - will restore motion mode:"
            << static_cast<int>(m_homingController->modeBeforeHoming());

    // SystemStateModel will handle:
    // 1. Setting homingState = Completed
    // 2. Restoring motionMode = modeBeforeHoming
    // 3. Clearing gotoHomePosition and homePositionReached flags
}

void GimbalController::onHomingFailed(const QString& reason) {
    qCritical() << "[GimbalController] Homing failed:" << reason;
    qWarning() << "[GimbalController] Gimbal position may be uncertain";

    // SystemStateModel will be notified of failure state
}

void GimbalController::onHomingAborted(const QString& reason) {
    qWarning() << "[GimbalController] Homing aborted:" << reason;
    qWarning() << "[GimbalController] Will restore motion mode:"
               << static_cast<int>(m_homingController->modeBeforeHoming());

    // SystemStateModel will handle flag clearing and mode restoration
}

// ============================================================================
// EMERGENCY STOP HANDLER
// ============================================================================

void GimbalController::processEmergencyStop(const SystemStateData& data) {
    bool emergencyActive = data.emergencyStopActive;

    // Detect rising edge (emergency stop activation)
    if (emergencyActive && !m_wasInEmergencyStop) {
        qCritical() << "";
        qCritical() << "========================================";
        qCritical() << "  EMERGENCY STOP ACTIVATED";
        qCritical() << "========================================";
        qCritical() << "";

        // Send STOP command to PLC42
        if (m_plc42 && m_plc42->data()->isConnected) {
            m_plc42->setStopGimbal();  // Sets gimbalOpMode = 1 -> Q1_4 HIGH (STOP)
            qCritical() << "[GimbalController] STOP command sent to Oriental Motor";
        } else {
            qCritical() << "[GimbalController] PLC42 not connected - cannot send STOP";
        }

        // Abort homing if in progress (delegated to HomingController)
        if (m_homingController->isHomingInProgress()) {
            qWarning() << "[GimbalController] Aborting in-progress homing sequence";
            m_homingController->abort("Emergency stop activated");
        }

        // Stop current motion mode servos
        if (m_currentMode) {
            m_currentMode->stopServos(this);
            qCritical() << "[GimbalController] All servo motion halted";
        }

        qCritical() << "[GimbalController] System halted - awaiting E-STOP release";

        m_wasInEmergencyStop = true;
    }
    // Detect falling edge (emergency stop release)
    else if (!emergencyActive && m_wasInEmergencyStop) {
        qInfo() << "";
        qInfo() << "========================================";
        qInfo() << "  Emergency stop CLEARED";
        qInfo() << "========================================";
        qInfo() << "";

        // Return to manual mode
        if (m_plc42 && m_plc42->data()->isConnected) {
            m_plc42->setManualMode();  // Sets gimbalOpMode = 0 -> Q1_4 LOW
            qInfo() << "[GimbalController] PLC42 returned to MANUAL mode";
        }

        qInfo() << "[GimbalController] System ready - operator may resume operations";
        qInfo() << "[GimbalController] Previous motion mode will be restored";

        m_wasInEmergencyStop = false;
    }
}

// ============================================================================
// FREE MODE HANDLER
// ============================================================================

void GimbalController::processFreeMode(const SystemStateData& data) {
    bool freeActive = data.freeGimbalState;

    // Detect rising edge (FREE mode activation)
    if (freeActive && !m_wasInFreeMode) {
        qInfo() << "";
        qInfo() << "========================================";
        qInfo() << "  FREE MODE ACTIVATED";
        qInfo() << "========================================";
        qInfo() << "";
        qInfo() << "[GimbalController] Local FREE toggle switch turned ON";
        qInfo() << "[GimbalController] Gimbal brakes RELEASED";
        qInfo() << "[GimbalController] Manual positioning enabled";
        qInfo() << "[GimbalController] No servo commands will be sent";
        qInfo() << "";
        qInfo() << "  CAUTION: Gimbal can be moved manually";
        qInfo() << "  Ensure area is clear before positioning";

        // Motion mode will be set to MotionFree by SystemStateModel
        // This handler just logs for operator awareness

        m_wasInFreeMode = true;
    }
    // Detect falling edge (FREE mode deactivation)
    else if (!freeActive && m_wasInFreeMode) {
        qInfo() << "";
        qInfo() << "========================================";
        qInfo() << "  FREE MODE DEACTIVATED";
        qInfo() << "========================================";
        qInfo() << "";
        qInfo() << "[GimbalController] Local FREE toggle switch turned OFF";
        qInfo() << "[GimbalController] Gimbal brakes ENGAGED";
        qInfo() << "[GimbalController] Returning to powered control";
        qInfo() << "[GimbalController] Previous motion mode will be restored";

        // Motion mode will be restored by SystemStateModel

        m_wasInFreeMode = false;
    }
}

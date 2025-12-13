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
QPointF calculateAngularOffsetFromPixelError(
    double errorPxX, double errorPxY,
    int imageWidthPx, int imageHeightPx,
    float cameraHfovDegrees)
{
    double angularOffsetXDeg = 0.0;
    double angularOffsetYDeg = 0.0;

    // Calculate azimuth offset
    // CRITICAL FIX: !!! check sign
    if (cameraHfovDegrees > 0.01f && imageWidthPx > 0) {
        double degreesPerPixelAz = cameraHfovDegrees / static_cast<double>(imageWidthPx);
        angularOffsetXDeg = - errorPxX * degreesPerPixelAz;
    }

    // Calculate elevation offset
    if (cameraHfovDegrees > 0.01f && imageWidthPx > 0 && imageHeightPx > 0) {
        double aspectRatio = static_cast<double>(imageWidthPx) / static_cast<double>(imageHeightPx);
        double vfov_rad_approx = 2.0 * std::atan(std::tan((cameraHfovDegrees * M_PI / 180.0) / 2.0) / aspectRatio);
        double vfov_deg_approx = vfov_rad_approx * 180.0 / M_PI;

        if (vfov_deg_approx > 0.01f) {
            double degreesPerPixelEl = vfov_deg_approx / static_cast<double>(imageHeightPx);
            // Invert Y: positive pixel error (target below) needs negative elevation (gimbal down)
            angularOffsetYDeg = errorPxY * degreesPerPixelEl;
        }
    }

    return QPointF(angularOffsetXDeg, angularOffsetYDeg);
}

} // namespace GimbalUtils

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

GimbalController::GimbalController(ServoDriverDevice* azServo,
                                   ServoDriverDevice* elServo,
                                   Plc42Device* plc42,
                                   SystemStateModel* stateModel,
                                   QObject* parent)
    : QObject(parent)
    , m_azServo(azServo)
    , m_elServo(elServo)
    , m_plc42(plc42)
    , m_stateModel(stateModel)
{
    // Initialize default motion mode
    setMotionMode(MotionMode::Idle);

    // Connect to system state changes
    // Direct connection (Qt::AutoConnection) - all components in main thread due to QModbus
    // Previous Qt::QueuedConnection was causing event queue saturation and latency issues
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &GimbalController::onSystemStateChanged);
    }

    // Connect alarm signals
    connect(m_azServo, &ServoDriverDevice::alarmDetected,
            this, &GimbalController::onAzAlarmDetected);
    connect(m_azServo, &ServoDriverDevice::alarmCleared,
            this, &GimbalController::onAzAlarmCleared);
    connect(m_elServo, &ServoDriverDevice::alarmDetected,
            this, &GimbalController::onElAlarmDetected);
    connect(m_elServo, &ServoDriverDevice::alarmCleared,
            this, &GimbalController::onElAlarmCleared);

    // Start periodic update timer (20Hz)
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &GimbalController::update);
    m_updateTimer->start(50);

    // Initialize centralized dt measurement timer (Expert Review Fix)
    m_velocityTimer.start();

    // Initialize homing timeout timer
    m_homingTimeoutTimer = new QTimer(this);
    m_homingTimeoutTimer->setSingleShot(true);
    connect(m_homingTimeoutTimer, &QTimer::timeout,
            this, &GimbalController::onHomingTimeout);

    qDebug() << "[GimbalController] Initialized (homing timeout:" << HOMING_TIMEOUT_MS << "ms)";
}

GimbalController::~GimbalController()
{
    shutdown();
}

// ============================================================================
// SHUTDOWN
// ============================================================================

void GimbalController::shutdown()
{
    if (m_currentMode) {
        m_currentMode->exitMode(this);
    }
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
}

// ============================================================================
// UPDATE LOOP
// ============================================================================

void GimbalController::update()
{
    // ========================================================================
    // Startup Sanity Checks (run once on first update)
    // ========================================================================
    static bool sanityChecksPerformed = false;
    if (!sanityChecksPerformed) {
        SystemStateData state = m_stateModel->data();

        // Verify IMU gyro rates are in deg/s (not rad/s)
        // Expected: typical vehicle rotation rates are 0-100 deg/s
        // If units were rad/s, typical values would be 0-1.75 rad/s
        const double MAX_REASONABLE_GYRO_DEGS = 500.0;  // deg/s (aggressive maneuver)
        const double MAX_REASONABLE_GYRO_RADS = 10.0;   // rad/s (~573 deg/s, clearly wrong)

        if (state.imuConnected) {
            double gyroMag = std::sqrt(state.GyroX * state.GyroX +
                                       state.GyroY * state.GyroY +
                                       state.GyroZ * state.GyroZ);

            if (gyroMag > MAX_REASONABLE_GYRO_DEGS) {
                qWarning() << "[GimbalController] IMU gyro rates suspiciously high:"
                           << "mag =" << gyroMag << "deg/s"
                           << "- Check units (should be deg/s, not rad/s)";
            } else if (gyroMag > 0.001 && gyroMag < MAX_REASONABLE_GYRO_RADS) {
                qWarning() << "[GimbalController] IMU gyro rates suspiciously low:"
                           << "mag =" << gyroMag
                           << "- Check units (should be deg/s, not rad/s)";
            }
        }

        // Verify stabilizer limits relationships
        const auto& stabCfg = MotionTuningConfig::instance().stabilizer;
        double componentSum = stabCfg.maxPositionVel + stabCfg.maxVelocityCorr;
        if (stabCfg.maxTotalVel < componentSum) {
            qWarning() << "[GimbalController] Stabilizer limit mismatch:"
                       << "maxTotalVel (" << stabCfg.maxTotalVel << ") < "
                       << "maxPositionVel + maxVelocityCorr (" << componentSum << ")"
                       << "- Position correction will saturate early";
        }

        sanityChecksPerformed = true;
        qDebug() << "[GimbalController] Startup sanity checks completed";
    }

    // ========================================================================
    // Centralized dt Computation (Expert Review Fix)
    // ========================================================================
    double dt = m_velocityTimer.restart() / 1000.0;  // Convert ms to seconds
    dt = std::clamp(dt, 0.001, 0.050);               // Clamp between 1-50 ms

    // ========================================================================
    // MANUAL MODE LAC SUPPORT (CROWS-like behavior)
    // ========================================================================
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
    // ========================================================================
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
        if (currentState.motionMode == MotionMode::Manual &&
            currentState.leadAngleCompensationActive) {
            // Use gimbal angular velocity as "target angular rate" for LAC
            // This matches CROWS behavior where operator tracking speed is used
            m_stateModel->updateTargetAngularRates(
                static_cast<float>(filteredGimbalVelAz),
                static_cast<float>(filteredGimbalVelEl)
            );

            // Debug output (throttled)
            static int manualLacLogCounter = 0;
            if (++manualLacLogCounter % 40 == 0) {  // Every 2 seconds @ 20Hz
                qDebug() << "[GimbalController] MANUAL MODE LAC:"
                         << "Gimbal velocity Az:" << filteredGimbalVelAz << "°/s"
                         << "El:" << filteredGimbalVelEl << "°/s";
            }
        }
    }

    // Store current position for next cycle's velocity calculation
    m_prevGimbalAz = currentState.gimbalAz;
    m_prevGimbalEl = currentState.gimbalEl;
    m_gimbalVelocityInitialized = true;

    // ========================================================================
    // Update Loop Timing Diagnostics
    // ========================================================================
    static auto lastUpdateTime = std::chrono::high_resolution_clock::now();
    static std::vector<long long> updateIntervals;
    static int updateCount = 0;

    auto now = std::chrono::high_resolution_clock::now();
    auto interval = std::chrono::duration_cast<std::chrono::microseconds>(now - lastUpdateTime).count();

    updateIntervals.push_back(interval);
    if (updateIntervals.size() > 100) {
        updateIntervals.erase(updateIntervals.begin());
    }

    if (++updateCount % 50 == 0) {
        long long minInterval = *std::min_element(updateIntervals.begin(), updateIntervals.end());
        long long maxInterval = *std::max_element(updateIntervals.begin(), updateIntervals.end());
        long long avgInterval = std::accumulate(updateIntervals.begin(), updateIntervals.end(), 0LL)
                                / static_cast<long long>(updateIntervals.size());
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

    // ========================================================================
    // Motion Mode Update
    // ========================================================================
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

void GimbalController::onSystemStateChanged(const SystemStateData& newData)
{
    // ========================================================================
    // Performance Optimization: Early exit checks
    // Only process expensive operations if relevant state has changed
    // This prevents wasting CPU on servo position updates (110 Hz!)
    // ========================================================================

    // PRIORITY 1: EMERGENCY STOP (highest priority!)
    if (newData.emergencyStopActive != m_oldState.emergencyStopActive) {
        processEmergencyStop(newData);
    }

    // If emergency stop active, skip all other processing
    if (newData.emergencyStopActive) {
        m_oldState = newData;
        return;
    }

    // PRIORITY 2: HOMING SEQUENCE
    if (newData.homingState != m_oldState.homingState ||
        newData.gotoHomePosition != m_oldState.gotoHomePosition) {
        processHomingSequence(newData);
    }

    // If homing in progress, skip motion mode changes to avoid interference
    if (newData.homingState == HomingState::InProgress ||
        newData.homingState == HomingState::Requested) {
        m_oldState = newData;
        return;
    }

    // PRIORITY 3: FREE MODE MONITORING
    if (newData.freeGimbalState != m_oldState.freeGimbalState) {
        processFreeMode(newData);
    }

    // ========================================================================
    // Motion Mode Change Detection
    // ========================================================================
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

    // ========================================================================
    // Tracking Target Updates (Performance: only when tracking data changed)
    // ========================================================================
    if (newData.motionMode == MotionMode::AutoTrack && m_currentMode) {
        if (dynamic_cast<TrackingMotionMode*>(m_currentMode.get())) {
            bool trackingDataChanged = (
                newData.trackerHasValidTarget != m_oldState.trackerHasValidTarget ||
                newData.trackedTargetCenterX_px != m_oldState.trackedTargetCenterX_px ||
                newData.trackedTargetCenterY_px != m_oldState.trackedTargetCenterY_px ||
                newData.trackedTargetVelocityX_px_s != m_oldState.trackedTargetVelocityX_px_s ||
                newData.trackedTargetVelocityY_px_s != m_oldState.trackedTargetVelocityY_px_s
            );

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
                        errorPxX, errorPxY,
                        newData.currentImageWidthPx, newData.currentImageHeightPx, activeHfov
                    );

                    // Target gimbal position = current position + offset
                    double targetGimbalAz = newData.gimbalAz + angularOffset.x();
                    double targetGimbalEl = newData.gimbalEl + angularOffset.y();

                    QPointF angularVelocity = GimbalUtils::calculateAngularOffsetFromPixelError(
                        newData.trackedTargetVelocityX_px_s,
                        newData.trackedTargetVelocityY_px_s,
                        newData.currentImageWidthPx,
                        newData.currentImageHeightPx,
                        activeHfov
                    );
                    double targetAngularVelAz_dps = angularVelocity.x();
                    double targetAngularVelEl_dps = angularVelocity.y();

                    // ================================================================
                    // BUG FIX #1: Update target angular rates for LAC calculation
                    // ================================================================
                    // Store angular rates in SystemStateData so WeaponController
                    // can use them to calculate motion lead (rate × TOF).
                    // Without this, LAC motion lead is always zero!
                    // ================================================================
                    m_stateModel->updateTargetAngularRates(
                        static_cast<float>(targetAngularVelAz_dps),
                        static_cast<float>(targetAngularVelEl_dps)
                    );

                    // Emit queued signal (thread-safe, prevents race conditions)
                    emit trackingTargetUpdated(
                        targetGimbalAz, targetGimbalEl,
                        targetAngularVelAz_dps, targetAngularVelEl_dps,
                        true
                    );
                } else {
                    // Target lost - clear angular rates and emit invalid signal
                    m_stateModel->updateTargetAngularRates(0.0f, 0.0f);
                    emit trackingTargetUpdated(0, 0, 0, 0, false);
                }
            }
        }
    } else if (m_oldState.motionMode == MotionMode::AutoTrack &&
               newData.motionMode != MotionMode::AutoTrack) {
        // ================================================================
        // BUG FIX #1: Clear angular rates when exiting tracking mode
        // ================================================================
        // Ensures rates are zeroed when tracking stops, so LAC doesn't
        // continue using stale velocity data.
        // ================================================================
        m_stateModel->updateTargetAngularRates(0.0f, 0.0f);
    }

    // ========================================================================
    // MANUAL MODE LAC: Clear angular rates when exiting Manual mode or
    // when LAC is deactivated while in Manual mode
    // ========================================================================
    bool wasManualWithLAC = (m_oldState.motionMode == MotionMode::Manual &&
                             m_oldState.leadAngleCompensationActive);
    bool isManualWithLAC = (newData.motionMode == MotionMode::Manual &&
                            newData.leadAngleCompensationActive);

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

void GimbalController::setMotionMode(MotionMode newMode)
{
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
    case MotionMode::ManualTrack:
    {
        auto trackingMode = std::make_unique<TrackingMotionMode>();
        // Direct connection - motion mode runs in same thread as GimbalController
        connect(this, &GimbalController::trackingTargetUpdated,
                trackingMode.get(), &TrackingMotionMode::onTargetPositionUpdated);
        m_currentMode = std::move(trackingMode);
        break;
    }

    case MotionMode::RadarSlew:
        m_currentMode = std::make_unique<RadarSlewMotionMode>();
        break;

    case MotionMode::AutoSectorScan:
    {
        auto scanMode = std::make_unique<AutoSectorScanMotionMode>();
        const auto& scanZones = m_stateModel->data().sectorScanZones;
        int activeId = m_stateModel->data().activeAutoSectorScanZoneId;

        auto it = std::find_if(scanZones.begin(), scanZones.end(),
                               [activeId](const AutoSectorScanZone& z) {
                                   return z.id == activeId && z.isEnabled;
                               });

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

    case MotionMode::TRPScan:
    {
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

    qDebug() << "[GimbalController] Motion mode set to" << static_cast<int>(m_currentMotionModeType);
}

// ============================================================================
// ALARM MANAGEMENT
// ============================================================================

void GimbalController::readAlarms()
{
    if (m_azServo) {
        m_azServo->readAlarmStatus();
    }
    if (m_elServo) {
        m_elServo->readAlarmStatus();
    }
}

void GimbalController::clearAlarms()
{
    // Clear PLC42 alarm state with two-step reset sequence
    m_plc42->setResetAlarm(0);

    // Second reset command after 1 second delay
    QTimer::singleShot(1000, this, [this]() {
        m_plc42->setResetAlarm(1);
    });
}

void GimbalController::onAzAlarmDetected(uint16_t alarmCode, const QString& description)
{
    emit azAlarmDetected(alarmCode, description);
}

void GimbalController::onAzAlarmCleared()
{
    emit azAlarmCleared();
}

void GimbalController::onElAlarmDetected(uint16_t alarmCode, const QString& description)
{
    emit elAlarmDetected(alarmCode, description);
}

void GimbalController::onElAlarmCleared()
{
    emit elAlarmCleared();
}

// ============================================================================
// HOMING STATE MACHINE
// ============================================================================

void GimbalController::processHomingSequence(const SystemStateData& data)
{
    switch (data.homingState) {
    case HomingState::Idle:
        // Check if home button pressed (rising edge detection)
        if (data.gotoHomePosition && !m_oldState.gotoHomePosition) {
            qInfo() << "[GimbalController] Home button pressed";
            startHomingSequence();
        }
        break;

    case HomingState::Requested:
    {
        // Send HOME command to PLC42
        if (m_plc42 && m_plc42->data()->isConnected) {
            qInfo() << "[GimbalController] Starting HOME sequence";

            // Store current mode to restore after homing completes
            m_modeBeforeHoming = data.motionMode;
            qDebug() << "[GimbalController] Stored current mode:"
                     << static_cast<int>(m_modeBeforeHoming);

            // Send HOME command to Oriental Motor via PLC42
            m_plc42->setHomePosition();  // Sets gimbalOpMode = 3 -> Q1_1 HIGH (ZHOME)

            // Update state to InProgress
            m_currentHomingState = HomingState::InProgress;

            // Start timeout timer (30 seconds)
            m_homingTimeoutTimer->start(HOMING_TIMEOUT_MS);

            qInfo() << "[GimbalController] HOME command sent, waiting for HOME-END signals...";
        } else {
            qCritical() << "[GimbalController] Cannot start homing - PLC42 not connected";
            abortHomingSequence("PLC42 not connected");
        }
        break;
    }

    case HomingState::InProgress:
    {
        // Check if BOTH HOME-END signals received
        bool azHomeDone = data.azimuthHomeComplete;
        bool elHomeDone = data.elevationHomeComplete;

        // Log individual axis completion (rising edge detection)
        if (azHomeDone && !m_oldState.azimuthHomeComplete) {
            qInfo() << "[GimbalController] Azimuth axis homed (HOME-END received)";
        }
        if (elHomeDone && !m_oldState.elevationHomeComplete) {
            qInfo() << "[GimbalController] Elevation axis homed (HOME-END received)";
        }

        // Complete homing only when BOTH axes report HOME-END
        if (azHomeDone && elHomeDone &&
            (!m_oldState.azimuthHomeComplete || !m_oldState.elevationHomeComplete)) {
            qInfo() << "[GimbalController] BOTH axes at home position";
            completeHomingSequence();
        }

        // Check for emergency stop during homing
        if (data.emergencyStopActive) {
            qWarning() << "[GimbalController] Homing aborted by emergency stop";
            abortHomingSequence("Emergency stop activated");
        }
        break;
    }

    case HomingState::Completed:
        // Auto-clear completed state after one cycle
        if (m_currentHomingState == HomingState::Completed) {
            qDebug() << "[GimbalController] Clearing completed homing state";
            m_currentHomingState = HomingState::Idle;
        }
        break;

    case HomingState::Failed:
    case HomingState::Aborted:
        // Auto-clear failed/aborted state after one cycle
        if (m_currentHomingState != HomingState::Idle) {
            qDebug() << "[GimbalController] Clearing failed/aborted homing state";
            m_currentHomingState = HomingState::Idle;
        }
        break;
    }
}

void GimbalController::startHomingSequence()
{
    // This method is called when home button first pressed
    // It signals SystemStateModel to transition to HomingState::Requested
    // SystemStateModel will update the state, which triggers processHomingSequence

    m_currentHomingState = HomingState::Requested;

    qInfo() << "[GimbalController] Homing sequence initiated";
    qInfo() << "[GimbalController] -> Will send HOME command to Oriental Motor";
}

void GimbalController::completeHomingSequence()
{
    // Stop timeout timer
    m_homingTimeoutTimer->stop();

    // Return PLC42 to manual mode (gimbalOpMode = 0)
    if (m_plc42) {
        m_plc42->setManualMode();  // Clears HOME output (Q1_1 LOW)
        qDebug() << "[GimbalController] PLC42 returned to MANUAL mode";
    }

    // Mark as completed
    m_currentHomingState = HomingState::Completed;

    qInfo() << "[GimbalController] HOME sequence completed successfully";
    qInfo() << "[GimbalController] Gimbal at home position, ready for operation";
    qInfo() << "[GimbalController] Will restore motion mode:"
            << static_cast<int>(m_modeBeforeHoming);

    // SystemStateModel will handle:
    // 1. Setting homingState = Completed
    // 2. Restoring motionMode = m_modeBeforeHoming
    // 3. Clearing gotoHomePosition and homePositionReached flags
}

void GimbalController::abortHomingSequence(const QString& reason)
{
    // Stop timeout timer
    m_homingTimeoutTimer->stop();

    // Return PLC42 to manual mode
    if (m_plc42) {
        m_plc42->setManualMode();
        qDebug() << "[GimbalController] PLC42 returned to MANUAL mode after abort";
    }

    // Mark as aborted
    m_currentHomingState = HomingState::Aborted;

    qCritical() << "[GimbalController] HOME sequence aborted:" << reason;
    qWarning() << "[GimbalController] Gimbal position may be uncertain";
    qWarning() << "[GimbalController] Will restore motion mode:"
               << static_cast<int>(m_modeBeforeHoming);

    // SystemStateModel will handle flag clearing and mode restoration
}

void GimbalController::onHomingTimeout()
{
    qCritical() << "[GimbalController] HOME sequence TIMEOUT after"
                << HOMING_TIMEOUT_MS << "ms";
    qCritical() << "[GimbalController] HOME-END signal was NOT received";
    qCritical() << "[GimbalController] Possible causes:";
    qCritical() << "[GimbalController]   - Wiring issue (I0_7 not connected)";
    qCritical() << "[GimbalController]   - Oriental Motor fault";
    qCritical() << "[GimbalController]   - Mechanical obstruction";

    // Mark as failed
    m_currentHomingState = HomingState::Failed;

    // Return PLC42 to manual mode
    if (m_plc42) {
        m_plc42->setManualMode();
    }

    // SystemStateModel will be notified of failure state
}

// ============================================================================
// EMERGENCY STOP HANDLER
// ============================================================================

void GimbalController::processEmergencyStop(const SystemStateData& data)
{
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

        // Abort homing if in progress
        if (m_currentHomingState == HomingState::InProgress ||
            m_currentHomingState == HomingState::Requested) {
            qWarning() << "[GimbalController] Aborting in-progress homing sequence";
            abortHomingSequence("Emergency stop activated");
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

void GimbalController::processFreeMode(const SystemStateData& data)
{
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

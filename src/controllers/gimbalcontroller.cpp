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
    if (cameraHfovDegrees > 0.01f && imageWidthPx > 0) {
        double degreesPerPixelAz = cameraHfovDegrees / static_cast<double>(imageWidthPx);
        angularOffsetXDeg = errorPxX * degreesPerPixelAz;
    }

    // Calculate elevation offset
    if (cameraHfovDegrees > 0.01f && imageWidthPx > 0 && imageHeightPx > 0) {
        double aspectRatio = static_cast<double>(imageWidthPx) / static_cast<double>(imageHeightPx);
        double vfov_rad_approx = 2.0 * std::atan(std::tan((cameraHfovDegrees * M_PI / 180.0) / 2.0) / aspectRatio);
        double vfov_deg_approx = vfov_rad_approx * 180.0 / M_PI;

        if (vfov_deg_approx > 0.01f) {
            double degreesPerPixelEl = vfov_deg_approx / static_cast<double>(imageHeightPx);
            // Invert Y: positive pixel error (target below) needs negative elevation (gimbal down)
            angularOffsetYDeg = -errorPxY * degreesPerPixelEl;
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
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &GimbalController::onSystemStateChanged);
    }

    // Connect alarm signals
    connect(m_azServo, &ServoDriverDevice::alarmDetected, this, &GimbalController::onAzAlarmDetected);
    connect(m_azServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onAzAlarmCleared);
    connect(m_elServo, &ServoDriverDevice::alarmDetected, this, &GimbalController::onElAlarmDetected);
    connect(m_elServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onElAlarmCleared);

    // Start periodic update timer (20Hz)
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &GimbalController::update);
    m_updateTimer->start(50);
}

GimbalController::~GimbalController()
{
    shutdown();
}

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
// CONTROL METHODS
// ============================================================================

void GimbalController::update()
{
    if (!m_currentMode) {
        return;
    }

    // Update gyro bias based on latest stationary status
    m_currentMode->updateGyroBias(m_stateModel->data());

    // Execute motion mode update if safety conditions are met
    if (m_currentMode->checkSafetyConditions(this)) {
        m_currentMode->update(this);
    } else {
        // Stop servos if safety conditions fail
        m_currentMode->stopServos(this);
    }
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void GimbalController::onSystemStateChanged(const SystemStateData &newData)
{
    // Detect motion mode type change
    bool motionModeTypeChanged = (m_oldState.motionMode != newData.motionMode);
    bool scanParametersChanged = false;

    // Check for scan parameter changes (only if mode type unchanged)
    if (!motionModeTypeChanged) {
        if (newData.motionMode == MotionMode::AutoSectorScan &&
            m_oldState.activeAutoSectorScanZoneId != newData.activeAutoSectorScanZoneId) {
            qDebug() << "GimbalController: AutoSectorScanZone changed to" << newData.activeAutoSectorScanZoneId;
            scanParametersChanged = true;
        } else if (newData.motionMode == MotionMode::TRPScan &&
                   m_oldState.activeTRPLocationPage != newData.activeTRPLocationPage) {
            qDebug() << "GimbalController: TRPLocationPage changed to" << newData.activeTRPLocationPage;
            scanParametersChanged = true;
        }
    }

    // Update tracking target (thread-safe via queued signal)
    if (newData.motionMode == MotionMode::AutoTrack && m_currentMode) {
        if (dynamic_cast<TrackingMotionMode*>(m_currentMode.get())) {
            if (newData.trackerHasValidTarget) {
                float screenCenterX_px = newData.currentImageWidthPx / 2.0f;
                float screenCenterY_px = newData.currentImageHeightPx / 2.0f;

                double errorPxX = newData.trackedTargetCenterX_px - screenCenterX_px;
                double errorPxY = newData.trackedTargetCenterY_px - screenCenterY_px;

                // Get active camera FOV
                float activeHfov = newData.activeCameraIsDay ?
                                   static_cast<float>(newData.dayCurrentHFOV) :
                                   static_cast<float>(newData.nightCurrentHFOV);

                QPointF angularOffset = GimbalUtils::calculateAngularOffsetFromPixelError(
                    errorPxX, errorPxY,
                    newData.currentImageWidthPx, newData.currentImageHeightPx, activeHfov
                );

                // The desired target gimbal position is current gimbal position + this offset
                // (because the offset tells us how far to move FROM current to get target to center)
                double targetGimbalAz = newData.gimbalAz + angularOffset.x();
                double targetGimbalEl = newData.gimbalEl + angularOffset.y();

                QPointF angularVelocity = GimbalUtils::calculateAngularOffsetFromPixelError(
                    newData.trackedTargetVelocityX_px_s, // Use velocity in pixels/sec
                    newData.trackedTargetVelocityY_px_s,
                    newData.currentImageWidthPx,
                    newData.currentImageHeightPx,
                    activeHfov
                );
                double targetAngularVelAz_dps = angularVelocity.x();
                double targetAngularVelEl_dps = angularVelocity.y();

                // Emit queued signal (thread-safe, prevents race conditions)
                emit trackingTargetUpdated(
                    targetGimbalAz, targetGimbalEl,
                    targetAngularVelAz_dps, targetAngularVelEl_dps,
                    true
                );
            } else {
                // Target lost - emit invalid signal
                emit trackingTargetUpdated(0, 0, 0, 0, false);
            }
        }
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
        // Connect with queued signal for thread-safe target updates
        connect(this, &GimbalController::trackingTargetUpdated,
                trackingMode.get(), &TrackingMotionMode::onTargetPositionUpdated,
                Qt::QueuedConnection);
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
            qWarning() << "GimbalController: AutoSectorScan zone" << activeId << "not found or disabled";
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
            trpMode->setActiveTRPPage(pageToScan);
            m_currentMode = std::move(trpMode);
        } else {
            qWarning() << "GimbalController: No TRPs for page" << activePageNum;
            m_currentMode = nullptr;
            newMode = MotionMode::Idle;
        }
        break;
    }

    default:
        qWarning() << "GimbalController: Unknown motion mode" << int(newMode);
        m_currentMode = nullptr;
        break;
    }

    m_currentMotionModeType = newMode;

    // Enter new mode
    if (m_currentMode) {
        m_currentMode->enterMode(this);
    }

    qDebug() << "GimbalController: Motion mode set to" << int(m_currentMotionModeType);
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

void GimbalController::onAzAlarmDetected(uint16_t alarmCode, const QString &description)
{
    emit azAlarmDetected(alarmCode, description);
}

void GimbalController::onAzAlarmCleared()
{
    emit azAlarmCleared();
}

void GimbalController::onElAlarmDetected(uint16_t alarmCode, const QString &description)
{
    emit elAlarmDetected(alarmCode, description);
}

void GimbalController::onElAlarmCleared()
{
    emit elAlarmCleared();
}

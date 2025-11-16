#include "gimbalcontroller.h"
#include "motion_modes/manualmotionmode.h"
#include "motion_modes/trackingmotionmode.h"
#include "motion_modes/autosectorscanmotionmode.h"
#include "motion_modes/radarslewmotionmode.h"
#include "motion_modes/trpscanmotionmode.h"

#include "hardware/devices/servodriverdevice.h"
#include "hardware/devices/plc42device.h"
#include <QDebug>

namespace GimbalUtils { // Example namespace
    QPointF calculateAngularOffsetFromPixelError(
        double errorPxX, double errorPxY, // Error = tracked_pos_px - screen_center_px
        int imageWidthPx, int imageHeightPx,
        float cameraHfovDegrees)
    {
        double angularOffsetXDeg = 0.0;
        double angularOffsetYDeg = 0.0;

        if (cameraHfovDegrees > 0.01f && imageWidthPx > 0) {
            double degreesPerPixelAz = cameraHfovDegrees / static_cast<double>(imageWidthPx);
            angularOffsetXDeg = errorPxX * degreesPerPixelAz;
        }

        if (cameraHfovDegrees > 0.01f && imageWidthPx > 0 && imageHeightPx > 0) {
             double aspectRatio = static_cast<double>(imageWidthPx) / static_cast<double>(imageHeightPx);
             double vfov_rad_approx = 2.0 * std::atan(std::tan((cameraHfovDegrees * M_PI / 180.0) / 2.0) / aspectRatio);
             double vfov_deg_approx = vfov_rad_approx * 180.0 / M_PI;
             if (vfov_deg_approx > 0.01f) {
                double degreesPerPixelEl = vfov_deg_approx / static_cast<double>(imageHeightPx);
                // If positive errorPxY means target is visually BELOW center (larger Y pixel value),
                // and positive gimbal EL command moves gimbal UP, then we need a POSITIVE EL command
                // to move the gimbal (and reticle) down towards the target.
                // So the sign of the angular offset should match the sign of the pixel error
                // if positive gimbal EL moves the view UP.
                // Let's assume:
                // Positive angularOffsetXDeg means gimbal should move RIGHT.
                // Positive angularOffsetYDeg means gimbal should move UP.
                angularOffsetYDeg = -errorPxY * degreesPerPixelEl; // If positive Y pixel error is DOWN, and positive El is UP
             }
        }
        return QPointF(angularOffsetXDeg, angularOffsetYDeg);
    }


} // namespace GimbalUtils

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
    // Default motion mode
    setMotionMode(MotionMode::Idle);

    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this,         &GimbalController::onSystemStateChanged);
    }

    connect(m_azServo, &ServoDriverDevice::alarmDetected, this, &GimbalController::onAzAlarmDetected);
    connect(m_azServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onAzAlarmCleared);
    // ARCHIVE: docs/legacy-snippets.md#entry-2 (alarm history signals - may be useful for future diagnostics)
    connect(m_elServo, &ServoDriverDevice::alarmDetected, this, &GimbalController::onElAlarmDetected);
    connect(m_elServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onElAlarmCleared);
    // ARCHIVE: docs/legacy-snippets.md#entry-2 (alarm history signals - may be useful for future diagnostics)

    // Initialize and start the update timer
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

void GimbalController::onSystemStateChanged(const SystemStateData &newData)
{
    if (m_oldState.motionMode != newData.motionMode) {
        setMotionMode(newData.motionMode);
    }
    bool motionModeTypeChanged = (m_oldState.motionMode != newData.motionMode);
    bool scanParametersChanged = false;

    if (!motionModeTypeChanged) { // Only check scan params if mode type itself hasn't changed
        if (newData.motionMode == MotionMode::AutoSectorScan &&
            m_oldState.activeAutoSectorScanZoneId != newData.activeAutoSectorScanZoneId) {
            qDebug() << "GimbalController: Active AutoSectorScanZoneId changed to" << newData.activeAutoSectorScanZoneId << "while mode is active.";
            scanParametersChanged = true;
        } else if (newData.motionMode == MotionMode::TRPScan &&
                   m_oldState.activeTRPLocationPage != newData.activeTRPLocationPage) {
            qDebug() << "GimbalController: Active TRPLocationPage changed to" << newData.activeTRPLocationPage << "while mode is active.";
            scanParametersChanged = true;
        }
    }

    // --- Target Update for Active Tracking Mode ---
    // ✅ CRITICAL FIX: Use queued signal instead of direct call (Expert Review - Race Condition Fix)
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
                double targetAngularVelAz_dps = angularVelocity.x(); // degrees per second
                double targetAngularVelEl_dps = angularVelocity.y();

                // ✅ EMIT QUEUED SIGNAL instead of direct call - prevents race conditions
                emit trackingTargetUpdated(
                    targetGimbalAz, targetGimbalEl,
                    targetAngularVelAz_dps, targetAngularVelEl_dps,
                    true
                );
            } else {
                // Target is invalid or lost - emit queued signal with invalid flag
                emit trackingTargetUpdated(0, 0, 0, 0, false);
            }
        }
    }    
    // If mode type changed OR if scan parameters for an active scan mode changed
    if (motionModeTypeChanged || scanParametersChanged) {
        // setMotionMode will use newData (via m_stateModel->data()) to configure the new/reconfigured mode
        setMotionMode(newData.motionMode);
    }
    
    float aimAz = newData.gimbalAz; // Or newData.reticleAz
    float aimEl = newData.gimbalEl; // Or newData.reticleEl
    //float range = newData.lrfDistance > 0 ? newData.lrfDistance : -1.0f; // Use LRF if available

    bool inNTZ = m_stateModel->isPointInNoTraverseZone(aimAz, aimEl);
    if (newData.isReticleInNoTraverseZone != inNTZ) {
        m_stateModel->setPointInNoTraverseZone(inNTZ);
        qDebug() << "newData.isReticleInNoTraverseZone = inNTZ";
    }

    m_oldState = newData;
}

void GimbalController::update()
{
    if (!m_currentMode) {
        return;
    }

    // Update gyro bias before any motion mode update, as it depends on the latest stationary status
    m_currentMode->updateGyroBias(m_stateModel->data());

    // Centralized safety check. If conditions are not met (e.g., E-Stop),
    // the servos are stopped, and the mode's specific update logic is skipped.
    if (m_currentMode->checkSafetyConditions(this)) {
        m_currentMode->update(this);
    } else {
        // Stop servos if safety conditions suddenly fail
        m_currentMode->stopServos(this);
    }
}

void GimbalController::setMotionMode(MotionMode newMode)
{
    //if (newMode == m_currentMotionModeType)
       // return;
    if (newMode == m_currentMotionModeType) {
       // Optional: Could add logic here to re-configure an existing mode if parameters change,
       // but the current implementation of destroying and re-creating is safer.
       // return;
    }
    // Exit old mode if any
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
            // ✅ CRITICAL FIX: Connect with Qt::QueuedConnection to eliminate race conditions
            // Signal from vision thread → slot on controller thread (queued, thread-safe)
            connect(this, &GimbalController::trackingTargetUpdated,
                    trackingMode.get(), &TrackingMotionMode::onTargetPositionUpdated,
                    Qt::QueuedConnection);
            m_currentMode = std::move(trackingMode);
        }
        break;
    case MotionMode::RadarSlew:
        m_currentMode = std::make_unique<RadarSlewMotionMode>();
        break;
    case MotionMode::AutoSectorScan:
        {
            auto scanMode = std::make_unique<AutoSectorScanMotionMode>();
            const auto& scanZones = m_stateModel->data().sectorScanZones;
            int activeId = m_stateModel->data().activeAutoSectorScanZoneId; // Get from model
            auto it = std::find_if(scanZones.begin(), scanZones.end(),
                                   [activeId](const AutoSectorScanZone& z){ return z.id == activeId && z.isEnabled; });
            if (it != scanZones.end()) {
                scanMode->setActiveScanZone(*it); // Pass the specific zone object
                m_currentMode = std::move(scanMode);
            } else {
                qWarning() << "GimbalController: Could not find active AutoSectorScan zone ID" << activeId << "or it's disabled. Setting Idle.";
                m_currentMode = nullptr; newMode = MotionMode::Idle;
            }
        }
        break;
    case MotionMode::TRPScan:
        {
            auto trpMode = std::make_unique<TRPScanMotionMode>();
            const auto& allTrps = m_stateModel->data().targetReferencePoints;
            int activePageNum = m_stateModel->data().activeTRPLocationPage; // Get from model
            std::vector<TargetReferencePoint> pageToScan;
            for(const auto& trp : allTrps) {
                if (trp.locationPage == activePageNum) {
                    pageToScan.push_back(trp);
                }
            }
            if (!pageToScan.empty()) {
                trpMode->setActiveTRPPage(pageToScan);
                m_currentMode = std::move(trpMode);
            } else {
                qWarning() << "GimbalController: No TRPs for active page" << activePageNum << ". Setting Idle.";
                m_currentMode = nullptr; newMode = MotionMode::Idle;
            }
        }
        break;
 
 
    default:
        qWarning() << "Unknown motion mode:" << int(newMode);
        m_currentMode = nullptr;
        break;
    }

    m_currentMotionModeType = newMode;

    if (m_currentMode) {
        m_currentMode->enterMode(this);
    }

    qDebug() << "[GimbalController] Mode set to" << int(m_currentMotionModeType);
}

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
    // ARCHIVE: docs/legacy-snippets.md#entry-4 (old alarm clearing logic - direct servo calls)
    m_plc42->setResetAlarm(0); // Reset PLC42 alarm state
    // Delay 1 second before sending the second command
    QTimer::singleShot(1000, this, [this]() {
        m_plc42->setResetAlarm(1); // Second command after 1s
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

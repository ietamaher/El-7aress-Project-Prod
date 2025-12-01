#include "osdviewmodel.h"
#include <QDebug>
#include <cmath>

OsdViewModel::OsdViewModel(QObject *parent)
    : QObject(parent)
    , m_accentColor(70, 226, 165) // Default green
    , m_modeText("MODE: IDLE")
    , m_motionText("MOTION: MAN")
    , m_stabText("STAB: OFF")
    , m_cameraText("CAM: DAY")
    , m_speedText("100.0%")
    , m_azimuth(0.0f)
    , m_elevation(0.0f)
    , m_statusText("SYS: --- SAF NRD")
    , m_rateText("RATE: SINGLE SHOT")
    , m_lrfText("LRF: --- m")
    , m_fovText("FOV: 45.0°")
    , m_homingText("")              // ⭐ Homing text empty by default
    , m_homingVisible(false)        // ⭐ Homing not visible by default
    , m_trackingBox(0, 0, 0, 0)
    , m_trackingBoxVisible(false)
    , m_trackingBoxColor(Qt::yellow)
    , m_trackingBoxDashed(false)
    , m_isTrackingActive(false)
    , m_acquisitionBox(0, 0, 0, 0)
    , m_acquisitionBoxVisible(false)
    , m_reticleType(ReticleType::BoxCrosshair)
    , m_reticleOffsetX(0.0f)
    , m_reticleOffsetY(0.0f)
    , m_currentFov(45.0f)
    , m_ccipX(0.0f)
    , m_ccipY(0.0f)
    , m_ccipVisible(false)
    , m_ccipStatus("Off")
    , m_zeroingText("")
    , m_zeroingVisible(false)
    , m_environmentText("")
    , m_environmentVisible(true)
    , m_zoneWarningText("")
    , m_zoneWarningVisible(false)
    , m_leadAngleText("")
    , m_leadAngleVisible(false)
    , m_scanNameText("")
    , m_scanNameVisible(false)
    , m_sysCharged(false)
    , m_sysArmed(false)
    , m_sysReady(false)
    , m_fireMode(FireMode::SingleShot)
    , m_screenWidth(1024)
    , m_screenHeight(768)
    , m_lacActive(false)
    , m_rangeMeters(0.0f)
    , m_confidenceLevel(1.0f)
    , m_trackingConfidence(0.0f)
    , m_startupMessageText("")
    , m_startupMessageVisible(false)
    , m_errorMessageText("")
    , m_errorMessageVisible(false)
    , m_dayCameraConnected(false)
    , m_dayCameraError(false)
    , m_nightCameraConnected(false)
    , m_nightCameraError(false)
    , m_azServoConnected(false)
    , m_azFault(false)
    , m_elServoConnected(false)
    , m_elFault(false)
    , m_lrfConnected(false)
    , m_lrfFault(false)
    , m_lrfOverTemp(false)
    , m_actuatorConnected(false)
    , m_actuatorFault(false)
    , m_plc21Connected(false)
    , m_plc42Connected(false)
    , m_joystickConnected(false)

{
}

void OsdViewModel::setAccentColor(const QColor& color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}

void OsdViewModel::updateMode(OperationalMode mode)
{
    QString newText;
    switch (mode) {
    case OperationalMode::Idle: newText = "MODE: IDLE"; break;
    case OperationalMode::Surveillance: newText = "MODE: OBS"; break;
    case OperationalMode::Tracking: newText = "MODE: TRACKING"; break;
    case OperationalMode::Engagement: newText = "MODE: ENGAGE"; break;
    case OperationalMode::EmergencyStop: newText = "MODE: EMERGENCY STOP"; break;
    default: newText = "MODE: UNKNOWN"; break;
    }

    if (m_modeText != newText) {
        m_modeText = newText;
        emit modeTextChanged();
    }
}

void OsdViewModel::updateMotionMode(MotionMode mode)
{
    QString newText;
    switch (mode) {
    case MotionMode::Idle: newText = "MOTION: IDLE"; break;
    case MotionMode::Manual: newText = "MOTION: MAN"; break;
    case MotionMode::AutoSectorScan: newText = "MOTION: SCAN"; break;
    case MotionMode::TRPScan: newText = "MOTION: TRP"; break;
    case MotionMode::ManualTrack: newText = "MOTION: TRACK"; break;
    case MotionMode::AutoTrack: newText = "MOTION: AUTO TRACK"; break;
    case MotionMode::RadarSlew: newText = "MOTION: RADAR"; break;
    case MotionMode::MotionFree: newText = "MOTION: FREE"; break;  // ⭐ BUG FIX: FREE mode now displayed
    default: newText = "MOTION: N/A"; break;
    }

    if (m_motionText != newText) {
        m_motionText = newText;
        emit motionTextChanged();
    }
}

// ============================================================================
// ⭐ HOMING STATE DISPLAY
// ============================================================================
void OsdViewModel::updateHomingState(HomingState state)
{
    QString newText;
    bool newVisible = false;

    switch (state) {
    case HomingState::Idle:
        newText = "";
        newVisible = false;
        break;
    case HomingState::Requested:
        newText = "[HOMING INIT]";
        newVisible = true;
        break;
    case HomingState::InProgress:
        newText = "[HOMING IN PROGRESS...]";
        newVisible = true;
        break;
    case HomingState::Completed:
        newText = "[HOMING COMPLETE]";
        newVisible = true;
        break;
    case HomingState::Failed:
        newText = "[HOMING FAILED!]";
        newVisible = true;
        break;
    case HomingState::Aborted:
        newText = "[HOMING ABORTED]";
        newVisible = true;
        break;
    default:
        newText = "";
        newVisible = false;
        break;
    }

    if (m_homingText != newText) {
        m_homingText = newText;
        emit homingTextChanged();
    }

    if (m_homingVisible != newVisible) {
        m_homingVisible = newVisible;
        emit homingVisibleChanged();
    }
}

void OsdViewModel::updateStabilization(bool enabled)
{
    QString newText = enabled ? "STAB: ON" : "STAB: OFF";
    if (m_stabText != newText) {
        m_stabText = newText;
        emit stabTextChanged();
    }
}

void OsdViewModel::updateCameraType(const QString& type)
{
    QString newText = QString("CAM: %1").arg(type.toUpper());
    if (m_cameraText != newText) {
        m_cameraText = newText;
        emit cameraTextChanged();
    }
}

void OsdViewModel::updateSpeed(double speed)
{
    QString newText = QString("%1%").arg(speed, 0, 'f', 1);
    if (m_speedText != newText) {
        m_speedText = newText;
        emit speedTextChanged();
    }
}

void OsdViewModel::updateAzimuth(float azimuth)
{
    // Normalize azimuth to 0-360 range
    while (azimuth < 0.0f) azimuth += 360.0f;
    while (azimuth >= 360.0f) azimuth -= 360.0f;

    if (m_azimuth != azimuth) {
        m_azimuth = azimuth;
        emit azimuthChanged();
    }
}

void OsdViewModel::updateElevation(float elevation)
{
    if (m_elevation != elevation) {
        m_elevation = elevation;
        emit elevationChanged();
    }
}

void OsdViewModel::updateImuData(bool connected, double yaw, double pitch, double roll, double temp)
    {
        bool changed = false;

        if (m_imuConnected != connected) {
            m_imuConnected = connected;
            emit imuConnectedChanged();
            changed = true;
        }

        if (!qFuzzyCompare(m_vehicleHeading, yaw)) {
            m_vehicleHeading = yaw;
            emit vehicleHeadingChanged();
            changed = true;
        }

        if (!qFuzzyCompare(m_vehicleRoll, roll)) {
            m_vehicleRoll = roll;
            emit vehicleRollChanged();
            changed = true;
        }

        if (!qFuzzyCompare(m_vehiclePitch, pitch)) {
            m_vehiclePitch = pitch;
            emit vehiclePitchChanged();
            changed = true;
        }

        if (!qFuzzyCompare(m_imuTemperature, temp)) {
            m_imuTemperature = temp;
            emit imuTemperatureChanged();
            changed = true;
        }

        if (changed) {
 
        }
    }

void OsdViewModel::updateSystemStatus(bool charged, bool armed, bool ready)
{
    m_sysCharged = charged;
    m_sysArmed = armed;
    m_sysReady = ready;

    QString newStatusText = QString("SYS: %1 %2 %3")
                                .arg(charged ? "CHG" : "---")
                                .arg(armed ? "ARM" : "SAF")
                                .arg(ready ? "RDY" : "NRD");

    if (m_statusText != newStatusText) {
        m_statusText = newStatusText;
        emit statusTextChanged();
    }
}

void OsdViewModel::updateFiringMode(FireMode mode)
{
    m_fireMode = mode;

    QString newRateText;
    switch (mode) {
    case FireMode::SingleShot: newRateText = "RATE: SINGLE SHOT"; break;
    case FireMode::ShortBurst: newRateText = "RATE: SHORT BURST"; break;
    case FireMode::LongBurst: newRateText = "RATE: LONG BURST"; break;
    default: newRateText = "RATE: UNKNOWN"; break;
    }

    if (m_rateText != newRateText) {
        m_rateText = newRateText;
        emit rateTextChanged();
    }
}

void OsdViewModel::updateLrfDistance(float distance)
{
    QString newText = (distance > 0.1f)
    ? QString::number(qRound(distance)) + " m"
    : "LRF: --- m";

    if (m_lrfText != newText) {
        m_lrfText = newText;
        emit lrfTextChanged();
    }
}

void OsdViewModel::updateFov(float fov)
{
    m_currentFov = fov;

    QString newText = QString("FOV: %1°").arg(fov, 0, 'f', 1);
    if (m_fovText != newText) {
        m_fovText = newText;
        emit fovTextChanged();
        emit currentFovChanged();
    }
}

// ============================================================================
// TRACKING UPDATES
// ============================================================================

void OsdViewModel::updateTrackingBox(float x, float y, float width, float height)
{
    QRectF newBox(x, y, width, height);
    bool newVisible = (width > 0 && height > 0);

    if (m_trackingBox != newBox) {
        m_trackingBox = newBox;
        emit trackingBoxChanged();
    }

    if (m_trackingBoxVisible != newVisible) {
        m_trackingBoxVisible = newVisible;
        emit trackingBoxVisibleChanged();
    }
}

void OsdViewModel::updateTrackingState(VPITrackingState state)
{
    QColor newColor;
    bool newDashed = false;

    switch (state) {
    case VPI_TRACKING_STATE_TRACKED:
        newColor = QColor(0, 255, 0); // Green - tracked
        newDashed = true;
        break;
    case VPI_TRACKING_STATE_LOST:
        newColor = QColor(255, 255, 0); // Yellow - lost
        newDashed = true;
        break;
    default:
        newColor = QColor(255, 255, 0); // Yellow - default
        newDashed = false;
        break;
    }

    if (m_trackingBoxColor != newColor) {
        m_trackingBoxColor = newColor;
        emit trackingBoxColorChanged();
    }

    if (m_trackingBoxDashed != newDashed) {
        m_trackingBoxDashed = newDashed;
        emit trackingBoxDashedChanged();
    }
}

void OsdViewModel::updateTrackingPhase(TrackingPhase phase, bool hasValidTarget, const QRectF& acquisitionBox)
{
    bool showAcquisition = false;
    bool showTracking = false;
    QColor boxColor = Qt::yellow;
    bool boxDashed = false;

    switch (phase) {
    case TrackingPhase::Acquisition:
        showAcquisition = true;
        showTracking = false;
        break;

    case TrackingPhase::Tracking_LockPending:
        showAcquisition = false;
        showTracking = true;
        boxColor = QColor(255, 255, 0); // Yellow - acquiring
        boxDashed = false;
        break;

    case TrackingPhase::Tracking_ActiveLock:
        showAcquisition = false;
        showTracking = hasValidTarget;
        boxColor = QColor(255, 0, 0); // Red - locked
        boxDashed = true;
        break;

    case TrackingPhase::Tracking_Coast:
        showAcquisition = false;
        showTracking = hasValidTarget;
        boxColor = QColor(255, 255, 0); // Yellow - coasting
        boxDashed = true;
        break;

    case TrackingPhase::Tracking_Firing:
        showAcquisition = false;
        showTracking = hasValidTarget;
        boxColor = QColor(0, 255, 0); // Green - firing
        boxDashed = true;
        break;

    case TrackingPhase::Off:
    default:
        showAcquisition = false;
        showTracking = false;
        break;
    }

    // Update acquisition box
    if (m_acquisitionBox != acquisitionBox) {
        m_acquisitionBox = acquisitionBox;
        emit acquisitionBoxChanged();

        if (showAcquisition) {
            qDebug() << "========================================";
            qDebug() << "ACQUISITION BOX UPDATE (to QML)";
            qDebug() << "========================================";
            qDebug() << "Top-Left:" << acquisitionBox.x() << "," << acquisitionBox.y();
            qDebug() << "Size:" << acquisitionBox.width() << "x" << acquisitionBox.height();
            qDebug() << "Center:" << (acquisitionBox.x() + acquisitionBox.width()/2.0)
                     << "," << (acquisitionBox.y() + acquisitionBox.height()/2.0);
            qDebug() << "========================================";
        }
    }

    if (m_acquisitionBoxVisible != showAcquisition) {
        m_acquisitionBoxVisible = showAcquisition;
        emit acquisitionBoxVisibleChanged();
        qDebug() << "Acquisition box visibility:" << (showAcquisition ? "VISIBLE" : "HIDDEN");
    }

    // Update tracking box visibility based on phase
    if (m_trackingBoxVisible != showTracking) {
        m_trackingBoxVisible = showTracking;
        emit trackingBoxVisibleChanged();
    }

    if (m_trackingBoxColor != boxColor) {
        m_trackingBoxColor = boxColor;
        emit trackingBoxColorChanged();
    }

    if (m_trackingBoxDashed != boxDashed) {
        m_trackingBoxDashed = boxDashed;
        emit trackingBoxDashedChanged();
    }
}

void OsdViewModel::updateTrackingActive(bool active)
{
    if (m_isTrackingActive != active) {
        m_isTrackingActive = active;
        emit isTrackingActiveChanged();
        emit trackingActiveChanged();  // Emit alias signal for QML compatibility
    }
}
// ============================================================================
// RETICLE UPDATES
// ============================================================================

void OsdViewModel::updateReticleType(ReticleType type)
{
    if (m_reticleType != type) {
        m_reticleType = type;
        emit reticleTypeChanged();
    }
}

void OsdViewModel::updateReticleOffset(float screen_x_px, float screen_y_px)
{
    // ✅ SIMPLE: Just convert absolute screen position to offset from center
    float centerX = m_screenWidth / 2.0f;
    float centerY = m_screenHeight / 2.0f;

    float offsetX = screen_x_px - centerX;
    float offsetY = screen_y_px - centerY;

    if (m_reticleOffsetX != offsetX || m_reticleOffsetY != offsetY) {
        m_reticleOffsetX = offsetX;
        m_reticleOffsetY = offsetY;
        emit reticleOffsetChanged();

        qDebug() << "========================================";
        qDebug() << "RETICLE POSITION UPDATE (to QML)";
        qDebug() << "========================================";
        qDebug() << "Screen Center:" << centerX << "," << centerY;
        qDebug() << "Reticle Absolute Position:" << screen_x_px << "," << screen_y_px;
        qDebug() << "Reticle Offset (from center):" << offsetX << "," << offsetY;
        qDebug() << "========================================";
    }
}

void OsdViewModel::updateCcipPipper(float screen_x_px, float screen_y_px, bool visible, const QString& status)
{
    // ========================================================================
    // CCIP PIPPER UPDATE
    // ========================================================================
    // Updates CCIP (Continuously Computed Impact Point) pipper position
    // This shows where bullets will impact with lead angle compensation
    //
    // Position is absolute screen coordinates (same as reticle input)
    // We store them directly for QML positioning
    // ========================================================================

    bool positionChanged = (m_ccipX != screen_x_px || m_ccipY != screen_y_px);
    bool visibilityChanged = (m_ccipVisible != visible);
    bool statusChanged = (m_ccipStatus != status);

    if (positionChanged || visibilityChanged || statusChanged) {
        m_ccipX = screen_x_px;
        m_ccipY = screen_y_px;
        m_ccipVisible = visible;
        m_ccipStatus = status;

        if (positionChanged) emit ccipPositionChanged();
        if (visibilityChanged) emit ccipVisibleChanged();
        if (statusChanged) emit ccipStatusChanged();

        if (visible) {
            qDebug() << "CCIP Pipper:"
                     << "Position(" << screen_x_px << "," << screen_y_px << ")"
                     << "Status:" << status;
        }
    }
}

// ============================================================================
// PROCEDURE UPDATES (Zeroing, Windage)
// ============================================================================

void OsdViewModel::updateZeroingDisplay(bool modeActive, bool applied, float azOffset, float elOffset)
{
    Q_UNUSED(azOffset);
    Q_UNUSED(elOffset);

    QString newText;
    bool newVisible = false;

    if (modeActive) {
        newText = "ZEROING";
        newVisible = true;
    } else if (applied) {
        newText = "Z";
        newVisible = true;
    }

    if (m_zeroingText != newText) {
        m_zeroingText = newText;
        emit zeroingTextChanged();
    }

    if (m_zeroingVisible != newVisible) {
        m_zeroingVisible = newVisible;
        emit zeroingVisibleChanged();
    }
}

void OsdViewModel::updateEnvironmentDisplay(float tempCelsius, float altitudeMeters)
{
    // Format environment parameters for display
    // NOTE: Wind is now handled by Windage menu (direction + speed),
    //       not environmental settings. Crosswind is calculated dynamically.
    QString newText = QString("T:%1°C  ALT:%2m")
                        .arg(tempCelsius, 0, 'f', 1)
                        .arg(altitudeMeters, 0, 'f', 0);

    if (m_environmentText != newText) {
        m_environmentText = newText;
        emit environmentTextChanged();
    }

    // Always visible
    if (!m_environmentVisible) {
        m_environmentVisible = true;
        emit environmentVisibleChanged();
    }
}

void OsdViewModel::updateWindageDisplay(bool windageApplied, float windSpeedKnots, float windDirectionDeg, float calculatedCrosswindMS)
{
    // Format windage information for display
    QString newText;
    bool newVisible = false;

    if (windageApplied && windSpeedKnots > 0.001f) {
        // Show wind direction, speed, and calculated crosswind component
        newText = QString("WIND: %1° %2kts  XWIND:%3m/s")
                    .arg(windDirectionDeg, 0, 'f', 0)
                    .arg(windSpeedKnots, 0, 'f', 1)
                    .arg(calculatedCrosswindMS, 0, 'f', 1);
        newVisible = true;
    }

    if (m_windageText != newText) {
        m_windageText = newText;
        emit windageTextChanged();
    }

    if (m_windageVisible != newVisible) {
        m_windageVisible = newVisible;
        emit windageVisibleChanged();
    }
}

void OsdViewModel::updateDetectionDisplay(bool enabled)
{
    QString newText;
    bool newVisible = false;

    if (enabled) {
        newText = "DETECTION: ACTIVE";
        newVisible = true;
    }

    if (m_detectionText != newText) {
        m_detectionText = newText;
        emit detectionTextChanged();
    }

    if (m_detectionVisible != newVisible) {
        m_detectionVisible = newVisible;
        emit detectionVisibleChanged();
    }
}

void OsdViewModel::updateDetectionBoxes(const std::vector<YoloDetection>& detections)
{
    // Convert YoloDetections to QVariantList for QML
    QVariantList newBoxes;

    for (const auto& det : detections) {
        QVariantMap box;
        box["x"] = det.box.x;
        box["y"] = det.box.y;
        box["width"] = det.box.width;
        box["height"] = det.box.height;
        box["className"] = QString::fromStdString(det.className);
        box["confidence"] = det.confidence;
        box["colorR"] = det.color.r;
        box["colorG"] = det.color.g;
        box["colorB"] = det.color.b;

        newBoxes.append(box);
    }

    // Always update (even if empty to clear old boxes)
    m_detectionBoxes = newBoxes;
    emit detectionBoxesChanged();
}

// ============================================================================
// ZONE & STATUS UPDATES
// ============================================================================

void OsdViewModel::updateZoneWarning(bool inNoFireZone, bool inNoTraverseLimit)
{
    QString newText;
    bool newVisible = false;

    if (inNoFireZone) {
        newText = "NO FIRE ZONE";
        newVisible = true;
    } else if (inNoTraverseLimit) {
        newText = "NO TRAVERSE LIMIT";
        newVisible = true;
    }

    if (m_zoneWarningText != newText) {
        m_zoneWarningText = newText;
        emit zoneWarningTextChanged();
    }

    if (m_zoneWarningVisible != newVisible) {
        m_zoneWarningVisible = newVisible;
        emit zoneWarningVisibleChanged();
    }
}

void OsdViewModel::updateLeadAngleDisplay(const QString& statusText)
{
    bool newVisible = !statusText.isEmpty();

    if (m_leadAngleText != statusText) {
        m_leadAngleText = statusText;
        emit leadAngleTextChanged();
    }

    if (m_leadAngleVisible != newVisible) {
        m_leadAngleVisible = newVisible;
        emit leadAngleVisibleChanged();
    }
}

void OsdViewModel::updateCurrentScanName(const QString& scanName)
{
    bool newVisible = !scanName.isEmpty();

    if (m_scanNameText != scanName) {
        m_scanNameText = scanName;
        emit scanNameTextChanged();
    }

    if (m_scanNameVisible != newVisible) {
        m_scanNameVisible = newVisible;
        emit scanNameVisibleChanged();
    }
}

void OsdViewModel::updateLacActive(bool active)
{
    if (m_lacActive != active) {
        m_lacActive = active;
        emit lacActiveChanged();
    }
}

void OsdViewModel::updateRangeMeters(float range)
{
    if (m_rangeMeters != range) {
        m_rangeMeters = range;
        emit rangeMetersChanged();
    }
}

void OsdViewModel::updateConfidenceLevel(float confidence)
{
    if (m_confidenceLevel != confidence) {
        m_confidenceLevel = confidence;
        emit confidenceLevelChanged();
    }
}

void OsdViewModel::updateTrackingConfidence(float confidence)
{
    if (m_trackingConfidence != confidence) {
        m_trackingConfidence = confidence;
        emit trackingConfidenceChanged();
    }
}

// ============================================================================
// STARTUP SEQUENCE & ERROR MESSAGE UPDATES
// ============================================================================

void OsdViewModel::updateStartupMessage(const QString& message, bool visible)
{
    bool changed = false;

    if (m_startupMessageText != message) {
        m_startupMessageText = message;
        emit startupMessageTextChanged();
        changed = true;
    }

    if (m_startupMessageVisible != visible) {
        m_startupMessageVisible = visible;
        emit startupMessageVisibleChanged();
        changed = true;
    }

    if (changed) {
        qDebug() << "[OsdViewModel] Startup message updated:"
                 << "Visible=" << visible
                 << "Text=" << message;
    }
}

void OsdViewModel::updateErrorMessage(const QString& message, bool visible)
{
    bool changed = false;

    if (m_errorMessageText != message) {
        m_errorMessageText = message;
        emit errorMessageTextChanged();
        changed = true;
    }

    if (m_errorMessageVisible != visible) {
        m_errorMessageVisible = visible;
        emit errorMessageVisibleChanged();
        changed = true;
    }

    if (changed) {
        qDebug() << "[OsdViewModel] Error message updated:"
                 << "Visible=" << visible
                 << "Text=" << message;
    }
}

void OsdViewModel::updateDeviceHealth(bool dayCamConnected, bool dayCamError,
                                      bool nightCamConnected, bool nightCamError,
                                      bool azServoConnected, bool azFault,
                                      bool elServoConnected, bool elFault,
                                      bool lrfConnected, bool lrfFault, bool lrfOverTemp,
                                      bool actuatorConnected, bool actuatorFault,
                                      bool imuConnected,
                                      bool plc21Connected, bool plc42Connected,
                                      bool joystickConnected)
{
    // Day Camera
    if (m_dayCameraConnected != dayCamConnected) {
        m_dayCameraConnected = dayCamConnected;
        emit dayCameraConnectedChanged();
    }
    if (m_dayCameraError != dayCamError) {
        m_dayCameraError = dayCamError;
        emit dayCameraErrorChanged();
    }

    // Night Camera
    if (m_nightCameraConnected != nightCamConnected) {
        m_nightCameraConnected = nightCamConnected;
        emit nightCameraConnectedChanged();
    }
    if (m_nightCameraError != nightCamError) {
        m_nightCameraError = nightCamError;
        emit nightCameraErrorChanged();
    }

    // Azimuth Servo
    if (m_azServoConnected != azServoConnected) {
        m_azServoConnected = azServoConnected;
        emit azServoConnectedChanged();
    }
    if (m_azFault != azFault) {
        m_azFault = azFault;
        emit azFaultChanged();
    }

    // Elevation Servo
    if (m_elServoConnected != elServoConnected) {
        m_elServoConnected = elServoConnected;
        emit elServoConnectedChanged();
    }
    if (m_elFault != elFault) {
        m_elFault = elFault;
        emit elFaultChanged();
    }

    // LRF
    if (m_lrfConnected != lrfConnected) {
        m_lrfConnected = lrfConnected;
        emit lrfConnectedChanged();
    }
    if (m_lrfFault != lrfFault) {
        m_lrfFault = lrfFault;
        emit lrfFaultChanged();
    }
    if (m_lrfOverTemp != lrfOverTemp) {
        m_lrfOverTemp = lrfOverTemp;
        emit lrfOverTempChanged();
    }

    // Actuator
    if (m_actuatorConnected != actuatorConnected) {
        m_actuatorConnected = actuatorConnected;
        emit actuatorConnectedChanged();
    }
    if (m_actuatorFault != actuatorFault) {
        m_actuatorFault = actuatorFault;
        emit actuatorFaultChanged();
    }

    // IMU (already has imuConnected property)
    if (m_imuConnected != imuConnected) {
        m_imuConnected = imuConnected;
        emit imuConnectedChanged();
    }

    // PLCs
    if (m_plc21Connected != plc21Connected) {
        m_plc21Connected = plc21Connected;
        emit plc21ConnectedChanged();
    }
    if (m_plc42Connected != plc42Connected) {
        m_plc42Connected = plc42Connected;
        emit plc42ConnectedChanged();
    }

    // Joystick
    if (m_joystickConnected != joystickConnected) {
        m_joystickConnected = joystickConnected;
        emit joystickConnectedChanged();
    }
}

void OsdViewModel::updateAmmunitionLevel(bool level)
{
    if (m_ammunitionLevel != level) {
        m_ammunitionLevel = level;
        emit ammunitionLevelChanged();
    }
}

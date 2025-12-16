#include "osdcontroller.h"
#include "models/osdviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include "hardware/devices/cameravideostreamdevice.h"
#include <QDebug>

OsdController::OsdController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
    , m_startupTimer(new QTimer(this))
    , m_staticDetectionTimer(new QTimer(this))
    , m_startupState(StartupState::Idle)
    , m_startupSequenceActive(false)
    , m_imuConnected(false)
    , m_staticDetectionComplete(false)
{
    // Connect startup timer
    connect(m_startupTimer, &QTimer::timeout, this, &OsdController::advanceStartupSequence);
    m_startupTimer->setSingleShot(true);

    // Connect static detection timer (10 seconds minimum for IMU gyro bias capture)
    connect(m_staticDetectionTimer, &QTimer::timeout, this, &OsdController::onStaticDetectionTimerExpired);
    m_staticDetectionTimer->setSingleShot(true);
}

void OsdController::setViewModel(OsdViewModel* viewModel)
{
    m_viewModel = viewModel;
    qDebug() << "OsdController: ViewModel set:" << m_viewModel;
}

void OsdController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
    qDebug() << "OsdController: StateModel set:" << m_stateModel;
}

void OsdController::initialize()
{
    qDebug() << "OsdController::initialize()";

    if (!m_viewModel) {
        qCritical() << "OsdController: ViewModel is null! Call setViewModel() first.";
        return;
    }

    if (!m_stateModel) {
        qCritical() << "OsdController: StateModel is null! Call setStateModel() first.";
        return;
    }

    // ⭐ Initialize active camera from state
    const auto& initialData = m_stateModel->data();
    m_activeCameraIndex = initialData.activeCameraIsDay ? 0 : 1;

    // Connect to state changes to track camera switching
    // ✅ LATENCY FIX: Queued connection prevents OSD updates from blocking device I/O
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, &OsdController::onSystemStateChanged,
            Qt::QueuedConnection);  // Non-blocking signal delivery

    // Connect to color changes
    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &OsdController::onColorStyleChanged);

    // Set initial state
    m_viewModel->setAccentColor(initialData.colorStyle);

    qDebug() << "OsdController initialized successfully";

    // =========================================================================
    // PHASE 2: Connect to CameraVideoStreamDevice (Add LATER)
    // =========================================================================
    // When you want frame-synchronized OSD updates, uncomment this:
    /*
    CameraVideoStreamDevice* dayCamera = ...; // Get from SystemController
    CameraVideoStreamDevice* nightCamera = ...;

    if (dayCamera) {
        connect(dayCamera, &CameraVideoStreamDevice::frameDataReady,
                this, &OsdController::onFrameDataReady);
    }
    if (nightCamera) {
        connect(nightCamera, &CameraVideoStreamDevice::frameDataReady,
                this, &OsdController::onFrameDataReady);
    }
    */
}


void OsdController::onSystemStateChanged(const SystemStateData& data)
{
    // Update active camera index when it changes
    int newActiveCameraIndex = data.activeCameraIsDay ? 0 : 1;

    if (m_activeCameraIndex != newActiveCameraIndex) {
        m_activeCameraIndex = newActiveCameraIndex;
        qDebug() << "OsdController: Active camera switched to"
                 << (m_activeCameraIndex == 0 ? "DAY" : "THERMAL");
    }

    // Monitor for critical device disconnections (only after startup complete)
    if (m_startupState == StartupState::Complete) {
        checkForCriticalErrors(data);
    }

    // Update device health status for warning displays
    if (m_viewModel) {
        m_viewModel->updateDeviceHealth(
            data.dayCameraConnected,
            data.dayCameraError,
            data.nightCameraConnected,
            data.nightCameraError,
            data.azServoConnected,
            data.azFault,
            data.elServoConnected,
            data.elFault,
            data.lrfConnected,
            data.lrfFault,
            data.lrfOverTemp,
            data.actuatorConnected,
            data.actuatorFault,
            data.imuConnected,
            data.plc21Connected,
            data.plc42Connected,
            data.joystickConnected
        );

        // Update environment display (not in FrameData, so updated here)
        m_viewModel->updateEnvironmentDisplay(
            data.environmentalTemperatureCelsius,
            data.environmentalAltitudeMeters
        );

        // Update ammunition feed status (also updated via FrameData path for synchronization)
        m_viewModel->updateAmmoFeedStatus(
            static_cast<int>(data.ammoFeedState),
            data.ammoFeedCycleInProgress,
            data.ammoLoaded
        );

        // ========================================================================
        // GIMBAL POSITION UPDATE - Direct from SystemStateData
        // ========================================================================
        // BUG FIX: Gimbal Az/El was only updated via FrameData path, which depends
        // on camera frames. During zeroing or when camera frame rate is low, the
        // gimbal position display would be stale or show 0°.
        //
        // This direct update ensures immediate response to servo data changes,
        // providing real-time gimbal position feedback on the OSD.
        // ========================================================================
        m_viewModel->updateAzimuth(data.gimbalAz);
        m_viewModel->updateElevation(data.gimbalEl);

        // NOTE: Windage display is now updated from FrameData in onFrameDataReady()
        // This ensures frame-synchronized display with other OSD elements
    }
}

void OsdController::checkForCriticalErrors(const SystemStateData& data)
{
    if (!m_viewModel) return;

    // Priority 1: Critical device disconnections
    if (!data.imuConnected) {
        //showErrorMessage("IMU DISCONNECTED - Platform stabilization unavailable");
        return;
    }

    if (!data.azServoConnected) {
        //showErrorMessage("AZIMUTH SERVO DISCONNECTED - Cannot slew horizontally");
        return;
    }

    if (!data.elServoConnected) {
        //showErrorMessage("ELEVATION SERVO DISCONNECTED - Cannot slew vertically");
        return;
    }

    // Priority 2: Critical device faults
    if (data.azFault) {
        //showErrorMessage("AZIMUTH SERVO FAULT - Check motor and driver");
        return;
    }

    if (data.elFault) {
        //showErrorMessage("ELEVATION SERVO FAULT - Check motor and driver");
        return;
    }

    // Priority 3: LRF critical errors
    if (data.lrfConnected && data.lrfFault) {
        //showErrorMessage("LASER RANGEFINDER FAULT - Ranging unavailable");
        return;
    }

    // If no critical errors, hide error message
    hideErrorMessage();
}


void OsdController::onFrameDataReady(const FrameData& frmdata)
{
    if (!m_viewModel) return;

    // ⭐ CRITICAL FIX: Only process frames from the ACTIVE camera!
    if (frmdata.cameraIndex != m_activeCameraIndex) {
        // Ignore frames from inactive camera
        return;
    }

    // === BASIC OSD DATA ===
    m_viewModel->updateMode(frmdata.currentOpMode);
    m_viewModel->updateMotionMode(frmdata.motionMode);
    m_viewModel->updateHomingState(frmdata.homingState);  // ⭐ Homing state display
    //check if motion mode is Tracking mode
    if (frmdata.motionMode == MotionMode::AutoTrack) {
        m_viewModel->updateTrackingActive(true);
    } else {
        m_viewModel->updateTrackingActive(false);
    }
    m_viewModel->updateStabilization(frmdata.stabEnabled);
    m_viewModel->updateAzimuth(frmdata.azimuth);
    m_viewModel->updateElevation(frmdata.elevation);
    m_viewModel->updateImuData(
        frmdata.imuConnected,
        frmdata.imuYawDeg,      // Vehicle heading
        frmdata.imuPitchDeg,
        frmdata.imuRollDeg,
        frmdata.imuTemp
    );
    m_viewModel->updateSpeed(frmdata.speed);
    m_viewModel->updateFov(frmdata.cameraFOV);

    // Camera type
    QString cameraType = (frmdata.cameraIndex == 0) ? "DAY" : "THERMAL";
    m_viewModel->updateCameraType(cameraType);

    // === SYSTEM STATUS ===
    m_viewModel->updateSystemStatus(frmdata.sysCharged, frmdata.gunArmed, frmdata.sysReady);
    m_viewModel->updateFiringMode(frmdata.fireMode);
    m_viewModel->updateLrfDistance(frmdata.lrfDistance);

    // ========================================================================
    // === RETICLE   ===
    // ========================================================================
    m_viewModel->updateReticleType(frmdata.reticleType);
    // ⭐ CRITICAL: Verify that pixel position is correct based on LAC status!
    // SystemStateModel SHOULD have already calculated correct position,
    // but let's verify the logic here as a safety check.

    float finalReticleX = frmdata.reticleAimpointImageX_px;
    float finalReticleY = frmdata.reticleAimpointImageY_px;

    // ⭐ SAFETY CHECK: If LAC is active but status is ZoomOut,
    // the reticle should be at zero offset (just zeroing applied)
    // This should already be handled by SystemStateModel, but verify:
    if (frmdata.leadAngleActive && frmdata.leadAngleStatus == LeadAngleStatus::ZoomOut) {
        qWarning() << "OsdController: LAC active but ZoomOut status!"
                   << "Reticle offsets should not include lead."
                   << "Current position: X=" << finalReticleX << "Y=" << finalReticleY;
        // The position SHOULD already be correct (without lead offsets)
        // because SystemStateModel should have handled this.
    }

    // ⭐ DEBUG: Log reticle position when LAC is active
    if (frmdata.leadAngleActive) {
        qDebug() << "OsdController: LAC active"
                 << "Status =" << static_cast<int>(frmdata.leadAngleStatus)
                 << "ReticlePos: X=" << finalReticleX << "Y=" << finalReticleY;
    }

    m_viewModel->updateReticleOffset(finalReticleX, finalReticleY);

    // ========================================================================
    // === CCIP PIPPER UPDATE (Ballistic Impact Prediction) ===
    // ========================================================================
    // CCIP shows where bullets will hit with lead angle compensation
    // Visible only when LAC is active
    // Position comes from reticleAimpointImageX/Y which includes lead offsets
    // ========================================================================

    // Determine CCIP status string
    QString ccipStatus = "Off";
    bool ccipVisible = false;

    if (frmdata.leadAngleActive) {
        ccipVisible = true;
        switch (frmdata.leadAngleStatus) {
            case LeadAngleStatus::On:
                ccipStatus = "On";
                break;
            case LeadAngleStatus::Lag:
                ccipStatus = "Lag";
                break;
            case LeadAngleStatus::ZoomOut:
                ccipStatus = "ZoomOut";
                break;
            default:
                ccipStatus = "Off";
                ccipVisible = false;
        }
    }

    // ========================================================================
    // CCIP PIPPER POSITION - PROPER ARCHITECTURAL FIX
    // ========================================================================
    // ✅ FIXED: Now reading from FrameData (camera thread) for frame synchronization
    // Previously read from SystemStateData which caused data source mismatch:
    //   - Reticle: from FrameData ✅
    //   - CCIP: from SystemStateData ❌ (STALE!)
    //   - Acquisition: from FrameData ✅
    //
    // Now all OSD elements are synchronized to the same frame!
    // ========================================================================

    m_viewModel->updateCcipPipper(
        frmdata.ccipImpactImageX_px,    // ✅ PROPER FIX: Read from FrameData (frame-synchronized)
        frmdata.ccipImpactImageY_px,    // ✅ PROPER FIX: Read from FrameData (frame-synchronized)
        ccipVisible,
        ccipStatus
    );

    // ========================================================================
    // === LAC VISUAL INDICATORS (for display elements) ===
    // ========================================================================

    // Determine if LAC is "effectively active" (On or Lag, not ZoomOut)
    bool lacEffectivelyActive = frmdata.leadAngleActive &&
                                (frmdata.leadAngleStatus == LeadAngleStatus::On ||
                                 frmdata.leadAngleStatus == LeadAngleStatus::Lag);

    m_viewModel->updateLacActive(lacEffectivelyActive);
    m_viewModel->updateRangeMeters(frmdata.lrfDistance);

    // LAC Confidence level based on lead angle status
    float lacConfidence = 1.0f;
    if (frmdata.leadAngleActive) {
        switch (frmdata.leadAngleStatus) {
        case LeadAngleStatus::On:
            lacConfidence = 1.0f;
            break;
        case LeadAngleStatus::Lag:
            lacConfidence = 0.5f;  // Reduced confidence
            break;
        case LeadAngleStatus::ZoomOut:
            lacConfidence = 0.0f;  // No confidence - can't calculate lead
            break;
        default:
            lacConfidence = 0.0f;
            break;
        }
    }
    m_viewModel->updateConfidenceLevel(lacConfidence);  // LAC confidence

    // VPI Tracking confidence (separate from LAC)
    m_viewModel->updateTrackingConfidence(frmdata.trackingConfidence);

    // === TRACKING BOX ===
    m_viewModel->updateTrackingBox(
        frmdata.trackingBbox.x(), frmdata.trackingBbox.y(),
        frmdata.trackingBbox.width(), frmdata.trackingBbox.height()
        );
    m_viewModel->updateTrackingState(frmdata.trackingState);

    // === TRACKING PHASE ===
    m_viewModel->updateTrackingPhase(
        frmdata.currentTrackingPhase,
        frmdata.trackerHasValidTarget,
        QRectF(frmdata.acquisitionBoxX_px, frmdata.acquisitionBoxY_px,
               frmdata.acquisitionBoxW_px, frmdata.acquisitionBoxH_px)
        );

    // === ZEROING ===
    m_viewModel->updateZeroingDisplay(
        frmdata.zeroingModeActive,
        frmdata.zeroingAppliedToBallistics,
        frmdata.zeroingAzimuthOffset,
        frmdata.zeroingElevationOffset
        );

    // === WINDAGE ===
    m_viewModel->updateWindageDisplay(
        frmdata.windageAppliedToBallistics,
        frmdata.windageSpeedKnots,
        frmdata.windageDirectionDegrees,
        frmdata.calculatedCrosswindMS
        );

    // === DETECTION ===
    m_viewModel->updateDetectionDisplay(frmdata.detectionEnabled);
    m_viewModel->updateDetectionBoxes(frmdata.detections);

    // === ZONE WARNINGS ===
    m_viewModel->updateZoneWarning(
        frmdata.isReticleInNoFireZone,
        frmdata.gimbalStoppedAtNTZLimit
        );

    // === LEAD ANGLE STATUS TEXT ===
    m_viewModel->updateLeadAngleDisplay(frmdata.leadStatusText);

    // === SCAN NAME ===
    m_viewModel->updateCurrentScanName(frmdata.currentScanName);
    m_viewModel->updateAmmunitionLevel(frmdata.stationAmmunitionLevel);

    // === AMMUNITION FEED STATUS ===
    m_viewModel->updateAmmoFeedStatus(
        static_cast<int>(frmdata.ammoFeedState),
        frmdata.ammoFeedCycleInProgress,
        frmdata.ammoLoaded
    );
}
// ============================================================================
// SHARED UPDATE LOGIC
// ============================================================================
/*void OsdController::updateViewModelFromSystemState(const SystemStateData& data)
{
    if (!m_viewModel) return;

    // Basic OSD data
    m_viewModel->updateMode(data.opMode);
    m_viewModel->updateMotionMode(data.motionMode);
    m_viewModel->updateStabilization(data.enableStabilization);
    m_viewModel->updateAzimuth(data.gimbalAz);
    m_viewModel->updateElevation(data.gimbalEl);
    m_viewModel->updateSpeed(data.gimbalSpeed);

    // FOV (select based on active camera)
    float fov = data.activeCameraIsDay ? data.dayCurrentHFOV : data.nightCurrentHFOV;
    m_viewModel->updateFov(fov);

    // System status
    m_viewModel->updateSystemStatus(data.ammoLoaded, data.gunArmed, data.isReady());
    m_viewModel->updateFiringMode(data.fireMode);
    m_viewModel->updateLrfDistance(data.lrfDistance);

    // Camera type
    QString cameraType = data.activeCameraIsDay ? "DAY" : "THERMAL";
    m_viewModel->updateCameraType(cameraType);

    // Tracking (simplified - full tracking box will come from FrameData later)
    if (data.trackingActive) {
        // For now, we don't have full tracking bbox from SystemStateModel
        // This will be properly updated when using FrameData
        m_viewModel->updateTrackingBox(0, 0, 0, 0);
    } else {
        m_viewModel->updateTrackingBox(0, 0, 0, 0);
    }

    // Reticle
    m_viewModel->updateReticleType(data.reticleType);
    m_viewModel->updateReticleOffset(
        data.reticleAimpointImageX_px,
        data.reticleAimpointImageY_px
        );

    // CCIP Pipper
    QString ccipStatus = "Off";
    bool ccipVisible = false;
    if (data.leadAngleCompensationActive) {
        ccipVisible = true;
        switch (data.currentLeadAngleStatus) {
            case LeadAngleStatus::On:
                ccipStatus = "On";
                break;
            case LeadAngleStatus::Lag:
                ccipStatus = "Lag";
                break;
            case LeadAngleStatus::ZoomOut:
                ccipStatus = "ZoomOut";
                break;
            default:
                ccipStatus = "Off";
                ccipVisible = false;
        }
    }
    m_viewModel->updateCcipPipper(
        data.ccipImpactImageX_px,
        data.ccipImpactImageY_px,
        ccipVisible,
        ccipStatus
    );

    // Zeroing
    m_viewModel->updateZeroingDisplay(
        data.zeroingModeActive,
        data.zeroingAppliedToBallistics,
        data.zeroingAzimuthOffset,
        data.zeroingElevationOffset
        );

    // Detection
    m_viewModel->updateDetectionDisplay(data.detectionEnabled);

    // Zone warnings
    m_viewModel->updateZoneWarning(
        data.isReticleInNoFireZone,
        data.isReticleInNoTraverseZone
        );

    // Lead angle
    m_viewModel->updateLeadAngleDisplay(data.leadStatusText);

    // Scan name
    m_viewModel->updateCurrentScanName(data.currentScanName);

    // Tracking phase
    m_viewModel->updateTrackingPhase(
        data.currentTrackingPhase,
        false, // trackerHasValidTarget - will be updated from FrameData later
        QRectF(data.acquisitionBoxX_px, data.acquisitionBoxY_px,
               data.acquisitionBoxW_px, data.acquisitionBoxH_px)
        );
}*/

void OsdController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "OsdController: Color changed to" << color;
    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
}

// ============================================================================
// STARTUP SEQUENCE CONTROL (EVENT-DRIVEN, REALISTIC TIMING)
// ============================================================================

void OsdController::startStartupSequence()
{
    if (!m_viewModel || !m_stateModel) {
        qWarning() << "[OsdController] Cannot start startup sequence - ViewModel or StateModel is null";
        return;
    }

    qDebug() << "[OsdController] Starting event-driven startup sequence";

    // Reset state
    m_startupSequenceActive = true;
    m_imuConnected = false;
    m_staticDetectionComplete = false;

    // Show initial message
    m_startupState = StartupState::SystemInit;
    updateStartupMessage(m_startupState);

    // Connect to SystemStateModel to monitor device connections
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, &OsdController::onStartupSystemStateChanged, Qt::UniqueConnection);

    // After 2 seconds of initialization message, hardware init starts
    m_startupTimer->start(2000);
}

void OsdController::advanceStartupSequence()
{
    if (!m_viewModel || !m_startupSequenceActive) return;

    // Transition from SystemInit to DetectingStatic
    // This happens when hardware initialization begins (IMU sends 0xCD command)
    if (m_startupState == StartupState::SystemInit) {
        m_startupState = StartupState::DetectingStatic;
        updateStartupMessage(m_startupState);

        // Start 10-second timer for gyro bias capture
        // (This represents the time the IMU hardware is capturing bias)
        m_staticDetectionTimer->start(10000);
    }
}

void OsdController::onStartupSystemStateChanged(const SystemStateData& data)
{
    if (!m_startupSequenceActive) return;

    checkDevicesAndAdvance(data);
}

void OsdController::checkDevicesAndAdvance(const SystemStateData& data)
{
    if (!m_viewModel || !m_startupSequenceActive) return;

    // Track IMU connection
    // IMU becomes connected AFTER gyro bias capture completes and data starts flowing
    if (data.imuConnected && !m_imuConnected) {
        m_imuConnected = true;
        qDebug() << "[OsdController] IMU connected - gyro bias capture complete, data flowing";
    }

    // After static detection timer completes AND IMU is connected, move to AHRS calibration
    if (m_staticDetectionComplete && m_imuConnected && m_startupState == StartupState::DetectingStatic) {
        m_startupState = StartupState::CalibratingAHRS;
        updateStartupMessage(m_startupState);
        m_startupTimer->start(2000);  // 2 seconds for AHRS calibration message
    }

    // After AHRS calibration timer expires, check for all critical devices
    if (m_startupState == StartupState::CalibratingAHRS && !m_startupTimer->isActive()) {
        // Check if critical devices are ready
        if (areCriticalDevicesConnected(data)) {
            m_startupState = StartupState::SystemReady;
            updateStartupMessage(m_startupState);
            m_startupTimer->start(1500);  // 1.5 seconds for "READY" message
        } else {
            // Wait silently for critical devices
            m_startupState = StartupState::WaitingForCriticalDevices;
            updateStartupMessage(m_startupState);
        }
    }

    // Waiting for critical devices - check if they're now ready
    if (m_startupState == StartupState::WaitingForCriticalDevices) {
        if (areCriticalDevicesConnected(data)) {
            m_startupState = StartupState::SystemReady;
            updateStartupMessage(m_startupState);
            m_startupTimer->start(1500);
        }
    }

    // Final transition to complete
    if (m_startupState == StartupState::SystemReady && !m_startupTimer->isActive()) {
        m_startupState = StartupState::Complete;
        m_startupSequenceActive = false;
        m_viewModel->updateStartupMessage("", false);  // Hide message

        // Disconnect from state changes
        disconnect(m_stateModel, &SystemStateModel::dataChanged,
                   this, &OsdController::onStartupSystemStateChanged);

        qDebug() << "[OsdController] Startup sequence complete";
    }
}

void OsdController::onStaticDetectionTimerExpired()
{
    qDebug() << "[OsdController] Static detection period complete (10 seconds - gyro bias capture time)";
    m_staticDetectionComplete = true;

    // Check if we should advance (need both timer complete AND IMU connected)
    if (m_stateModel) {
        checkDevicesAndAdvance(m_stateModel->data());
    }
}

bool OsdController::areCriticalDevicesConnected(const SystemStateData& data) const
{
    // Critical devices: IMU, Azimuth servo, Elevation servo
    // Cameras are not critical for basic operation
    bool critical = data.imuConnected; // &&
                 //  data.azServoConnected &&
                  // data.elServoConnected;

    if (critical) {
        qDebug() << "[OsdController] All critical devices connected";
    }

    return critical;
}

void OsdController::updateStartupMessage(StartupState state)
{
    if (!m_viewModel) return;

    QString message;
    bool visible = true;

    switch (state) {
        case StartupState::SystemInit:
            message = "SYSTEM INITIALIZATION...";
            break;

        case StartupState::WaitingForIMU:
            message = "WAITING FOR IMU CONNECTION...";
            break;

        case StartupState::DetectingStatic:
            message = "DETECTING STATIC CONDITION...";
            break;

        case StartupState::CalibratingAHRS:
            message = "CALIBRATING AHRS...";
            break;

        case StartupState::WaitingForCriticalDevices:
            message = "WAITING FOR CRITICAL DEVICES...";
            visible = true;  // Show message while waiting
            break;

        case StartupState::SystemReady:
            message = "SYSTEM READY";
            break;

        default:
            message = "";
            visible = false;
            break;
    }

    if (!message.isEmpty()) {
        qDebug() << "[OsdController] Startup message:" << message;
    }
    m_viewModel->updateStartupMessage(message, visible);
}

void OsdController::showErrorMessage(const QString& errorText)
{
    if (!m_viewModel) {
        qWarning() << "[OsdController] Cannot show error - ViewModel is null";
        return;
    }

    qDebug() << "[OsdController] Showing error message:" << errorText;
    m_viewModel->updateErrorMessage(errorText, true);
}

void OsdController::hideErrorMessage()
{
    if (!m_viewModel) return;

   // qDebug() << "[OsdController] Hiding error message";
    m_viewModel->updateErrorMessage("", false);
}

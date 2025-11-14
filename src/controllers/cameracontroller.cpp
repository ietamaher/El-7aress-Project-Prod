#include "cameracontroller.h"
#include "hardware/devices/daycameracontroldevice.h"
#include "hardware/devices/nightcameracontroldevice.h"
#include "models/domain/systemstatemodel.h"
#include "hardware/devices/cameravideostreamdevice.h" // Include the new processor header
#include "hardware/devices/lrfdevice.h"               // LRF device
#include "hardware/data/DataTypes.h"                  // For LrfData

#include <QDebug>
#include <QMetaObject> // For invokeMethod
#include <QMutexLocker>
#include <cmath>       // For std::sqrt etc. if needed, but removed handoff

CameraController::CameraController(DayCameraControlDevice* dayControl,
                                   CameraVideoStreamDevice* dayProcessor,
                                   NightCameraControlDevice* nightControl,
                                   CameraVideoStreamDevice* nightProcessor,
                                   SystemStateModel* stateModel,
                                   LRFDevice* lrfDevice,
                                   QObject* parent)
    : QObject(parent),
      m_dayControl(dayControl),
      m_dayProcessor(dayProcessor),     // Store processor pointer
      m_nightControl(nightControl),
      m_nightProcessor(nightProcessor), // Store processor pointer
      m_stateModel(stateModel),
      m_lrfDevice(lrfDevice),           // Initialize LRF device
      m_isDayCameraActive(true),        // Default to day camera
      m_lutIndex(0)                     // Default LUT index
{
    // Connect to system state changes - IMPORTANT!
    if (m_stateModel) {
        // Connect to the main dataChanged signal, or specific signals if SystemStateModel provides them
        // (e.g., activeCameraChanged(bool), trackingActiveChanged(bool))
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &CameraController::onSystemStateChanged, Qt::QueuedConnection); // Queued connection is safer

        // Initialize internal state from the model
        //m_stateModel->data() = m_stateModel->data();
        m_isDayCameraActive = m_stateModel->data().activeCameraIsDay;
        qInfo() << "CameraController initialized. Active camera is:" << (m_isDayCameraActive ? "Day" : "Night");
    } else {
        qWarning() << "CameraController created without a SystemStateModel!";
    }

    // ========================================================================
    // LRF INTEGRATION - Connect to device signal
    // ========================================================================
    if (m_lrfDevice) {
        // Connect directly to LRFDevice signal (using std::shared_ptr signature)
        connect(m_lrfDevice, &LRFDevice::lrfDataChanged,
                this, [this](std::shared_ptr<const LrfData> newData) {
                    if (newData) {
                        this->onLrfDataChanged(*newData);
                    }
                });
        qInfo() << "[CameraController] LRF device connected";
    } else {
        qWarning() << "[CameraController] LRF device not available!";
    }

    // Initialization logic moved to initialize() method
}

CameraController::~CameraController()
{
    qInfo() << "CameraController destructor";
    // Stop tracking might be attempted, but rely on main shutdown sequence primarily
    // This avoids potential issues if SystemStateModel is already gone
}

bool CameraController::initialize()
{
    // CameraVideoStreamDevice instances are started by SystemController
    // CameraControlDevices are opened by SystemController
    // LensDevice might be opened by SystemController

    // Check if essential components are present
    if (!m_stateModel || !m_dayControl || !m_nightControl || !m_dayProcessor || !m_nightProcessor) {
        updateStatus("Initialization failed: Missing required components.");
        return false;
    }

    // Perform any initial configuration needed for camera controls?
    // e.g., set default LUT, focus mode?

    updateStatus("CameraController initialized.");
    return true;
}


CameraVideoStreamDevice* CameraController::getDayCameraProcessor() const
{
    return m_dayProcessor;
}

CameraVideoStreamDevice* CameraController::getNightCameraProcessor() const
{
    return m_nightProcessor;
}

CameraVideoStreamDevice* CameraController::getActiveCameraProcessor() const
{
    // Use the cached m_isDayCameraActive flag for immediate response
    return m_isDayCameraActive ? m_dayProcessor : m_nightProcessor;
}

bool CameraController::isDayCameraActive() const
{
    return m_isDayCameraActive;
}


// Slot to react to the central state model changes
void CameraController::onSystemStateChanged(const SystemStateData &newData)
{
    QMutexLocker locker(&m_mutex); // Lock if modifying shared member data

    bool cameraChanged = (m_cachedState.activeCameraIsDay != newData.activeCameraIsDay);
    //bool trackingChanged = (m_cachedState.trackingActive != newData.trackingActive);
    //bool opModeChanged = (m_cachedState.opMode != newData.opMode);
    // ... check other relevant state changes using m_cachedState vs newData ...

    // Keep previous state temporarily *before* updating the cache
    SystemStateData oldStateBeforeUpdate = m_cachedState;

    // Update the internal cached state AFTER comparisons
    m_cachedState = newData;

    // --- React to Changes ---

    // 1. Active Camera Changed
    if (cameraChanged) {
        setActiveCamera(newData.activeCameraIsDay); // Update internal flag AND handle side effects

        // Stop tracking on the camera that just became *inactive*
        CameraVideoStreamDevice* oldProcessor = oldStateBeforeUpdate.activeCameraIsDay ? m_dayProcessor : m_nightProcessor;
        if (oldProcessor && oldStateBeforeUpdate.trackingActive) // Check if tracking WAS active on the old camera
        {
            qInfo() << "CameraController: Camera switched, stopping tracking on inactive processor:" << oldProcessor->property("cameraIndex").toInt();
            // Safely call the slot on the CameraVideoStreamDevice thread
            QMetaObject::invokeMethod(oldProcessor, "setTrackingEnabled", Qt::QueuedConnection,
                                      Q_ARG(bool, false));
        }
        emit stateChanged(); // Notify that the active camera processor changed
    }

    // 2. Tracking State Changed (React if needed, but start/stopTracking handles the action)
    // if (trackingChanged) {
    //    qInfo() << "CameraController: Tracking state changed in model to:" << newData.trackingActive;
    //    emit stateChanged();
    // }

    // 3. OpMode or MotionMode Changed (React if needed)
    // if (opModeChanged || ...) {
    //     emit stateChanged();
    // }

    locker.unlock(); // Unlock before emitting status potentially
    // Optional: Update status based on changes
    // updateStatus(...)
}

// Internal helper to update active camera flag
void CameraController::setActiveCamera(bool isDay)
{
    // Assumes mutex is already locked if called from onSystemStateChanged
    if (m_isDayCameraActive != isDay) {
        m_isDayCameraActive = isDay;
        qInfo() << "CameraController: Active camera set internally to:" << (isDay ? "Day" : "Night");
        // Don't emit stateChanged here, let onSystemStateChanged handle it after all checks
    }
}

// --- Tracking Control ---

bool CameraController::startTracking()
{
    QMutexLocker locker(&m_mutex); // Access state model safely
    if (!m_stateModel) {
        updateStatus("Cannot start tracking: SystemStateModel missing.");
        return false;
    }

    CameraVideoStreamDevice* activeProcessor = getActiveCameraProcessor(); // Use internal flag
    if (!activeProcessor) {
        updateStatus("Cannot start tracking: No active camera processor.");
        return false;
    }

    // Check if tracking is already considered active by the state model
    if (m_stateModel->data().trackingActive) {
        updateStatus("Tracking already active.");
        return true; // Or false depending on desired behaviour
    }
    locker.unlock(); // Unlock before invoking method

    qInfo() << "CameraController: Requesting tracking START on processor:" << activeProcessor->property("cameraIndex").toInt();

    // Tell the active processor to enable tracking
    bool invokeSuccess = QMetaObject::invokeMethod(activeProcessor, "setTrackingEnabled", Qt::QueuedConnection,
                                                   Q_ARG(bool, true));

    if (!invokeSuccess) {
         qWarning() << "CameraController: Failed to invoke setTrackingEnabled(true) on processor.";
         updateStatus("Failed to send start tracking command.");
         // Should we revert state model? Depends on system design.
         return false;
    }

    // IMPORTANT: Update the *central state model* AFTER successfully invoking the command
    // Let the model signal the change back via onSystemStateChanged for consistency
    if (m_stateModel) {
         m_stateModel->setTrackingStarted(true); // Tell the model tracking is requested/active
    }

    updateStatus(QString("Tracking start requested on %1 camera.").arg(m_isDayCameraActive ? "Day" : "Night"));
     // Don't emit stateChanged here, wait for the model's signal

    return true;
}

void CameraController::stopTracking()
{
    QMutexLocker locker(&m_mutex); // Access state model safely
    if (!m_stateModel) {
        updateStatus("Cannot stop tracking: SystemStateModel missing.");
        return;
    }

    CameraVideoStreamDevice* activeProcessor = getActiveCameraProcessor();
    if (!activeProcessor) {
        updateStatus("Cannot stop tracking: No active camera processor.");
        return;
    }

    // Check if tracking is actually active according to the state model
    if (!m_stateModel->data().trackingActive) {
        updateStatus("Tracking already stopped.");
        return; // Nothing to do
    }
    locker.unlock(); // Unlock before invoking method

    qInfo() << "CameraController: Requesting tracking STOP on processor:" << activeProcessor->property("cameraIndex").toInt();

    // Tell the active processor to disable tracking
    bool invokeSuccess = QMetaObject::invokeMethod(activeProcessor, "setTrackingEnabled", Qt::QueuedConnection,
                                                   Q_ARG(bool, false));

    if (!invokeSuccess) {
         qWarning() << "CameraController: Failed to invoke setTrackingEnabled(false) on processor.";
         updateStatus("Failed to send stop tracking command.");
         // Revert model state?
         return;
    }

    // Update the central state model AFTER successfully invoking the command
    if (m_stateModel) {
        m_stateModel->setTrackingStarted(false); // Tell the model tracking is stopped
    }

    updateStatus(QString("Tracking stop requested on %1 camera.").arg(m_isDayCameraActive ? "Day" : "Night"));
    // Don't emit stateChanged here, wait for the model's signal
}
/*
 *
 * /home/rapit/Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.cpp:198: error: invalid operands of types ‘const char [29]’ and ‘const char*’ to binary ‘operator+’
../../Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.cpp: In member function ‘bool CameraController::startTracking()’:
../../Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.cpp:198:49: error: invalid operands of types ‘const char [29]’ and ‘const char*’ to binary ‘operator+’
  198 |     updateStatus("Tracking start requested on " + (m_isDayCameraActive ? "Day" : "Night") + " camera.");
      |                  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ^ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      |                  |                                                     |
      |                  const char [29]                                       const char*
      */

// --- Camera Control Wrappers ---

void CameraController::zoomIn()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) m_dayControl->zoomIn();
    } else {
        // Handle night camera zoom (digital?) - Assuming setDigitalZoom exists
         if (m_nightControl) m_nightControl->setDigitalZoom(4); // Example increment
    }
}

void CameraController::zoomOut()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) m_dayControl->zoomOut();
    } else {
        // Handle night camera zoom
         if (m_nightControl) m_nightControl->setDigitalZoom(0); // Example decrement
    }
}

void CameraController::zoomStop()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) m_dayControl->zoomStop();
    } else {
        // Night camera digital zoom might not have a stop command
    }
}

void CameraController::focusNear()
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->focusNear();
    }
}

void CameraController::focusFar()
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->focusFar();
    }
}

void CameraController::focusStop()
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->focusStop();
    }
}

void CameraController::setFocusAuto(bool enabled)
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->setFocusAuto(enabled);
    }
}

void CameraController::nextVideoLUT()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_lutIndex++; // Increment internal index
        // Wrap around if necessary, assuming m_nightControl handles invalid indices gracefully or you know the count
        // if (m_lutIndex >= MAX_LUT_COUNT) m_lutIndex = 0;
        if (m_lutIndex >12) m_lutIndex = 12;
        m_nightControl->setVideoModeLUT(m_lutIndex);
    }
}

void CameraController::prevVideoLUT()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_lutIndex--;
        if (m_lutIndex < 0) m_lutIndex = 0; // Prevent negative index
        m_nightControl->setVideoModeLUT(m_lutIndex);
    }
}

void CameraController::performFFC()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_nightControl->performFFC();
    }
}

// --- Status Update ---
void CameraController::updateStatus(const QString& message)
{
    // Avoid redundant messages?
    if (statusMessage != message) {
        statusMessage = message;
        qDebug() << "CameraController Status:" << message;
        emit statusUpdated(message); // Emit signal for external listeners (like status bar)
    }
}

// ============================================================================
// LRF CONTROL METHODS (Laser Range Finder)
// ============================================================================
// Called by JoystickController when Button 1 is pressed
// ============================================================================

void CameraController::triggerLRF()
{
    if (!m_lrfDevice) {
        qWarning() << "[CameraController] LRF device not available!";
        return;
    }

    qInfo() << "[CameraController] LRF single shot triggered (Button 1)";
    m_lrfDevice->sendSingleRanging();
}

void CameraController::startContinuousLRF()
{
    if (!m_lrfDevice) {
        qWarning() << "[CameraController] LRF device not available!";
        return;
    }

    qInfo() << "[CameraController] LRF continuous ranging started (5Hz)";
    m_lrfDevice->sendContinuousRanging5Hz();
}

void CameraController::stopContinuousLRF()
{
    if (!m_lrfDevice) {
        qWarning() << "[CameraController] LRF device not available!";
        return;
    }

    qInfo() << "[CameraController] LRF ranging stopped";
    m_lrfDevice->stopRanging();
}

// ============================================================================
// LRF DATA HANDLER - Updates SystemStateModel with range measurements
// ============================================================================
// Connected to LRFDevice::lrfDataChanged signal in constructor
// This bridges sensor data (LRF) to fire control (WeaponController reads it)
// ============================================================================

void CameraController::onLrfDataChanged(const LrfData &newData)
{
    if (!m_stateModel) {
        qWarning() << "[CameraController] SystemStateModel not available!";
        return;
    }

    // Only update if ranging is valid and has echo
    if (newData.isLastRangingValid && newData.lastDistance > 0) {
        float range_m = static_cast<float>(newData.lastDistance);

        // Update central state (WeaponController reads currentTargetRange for ballistics)
        m_stateModel->updateTargetRange(range_m);

        qInfo() << "[CameraController] LRF measured:" << range_m << "m"
                << "- SystemStateModel updated for ballistics";
    } else {
        // LRF ranging failed - log the reason
        if (newData.noEcho) {
            qWarning() << "[CameraController] LRF: No echo detected (target too far or non-reflective)";
        } else if (newData.isFault) {
            qWarning() << "[CameraController] LRF: Device fault detected";
        } else if (newData.laserNotOut) {
            qWarning() << "[CameraController] LRF: Laser not firing";
        } else {
            qWarning() << "[CameraController] LRF: Invalid ranging result";
        }
    }
}

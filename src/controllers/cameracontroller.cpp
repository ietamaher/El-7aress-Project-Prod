#include "cameracontroller.h"

// ============================================================================
// Project
// ============================================================================
#include "hardware/devices/daycameracontroldevice.h"
#include "hardware/devices/nightcameracontroldevice.h"
#include "hardware/devices/cameravideostreamdevice.h"
#include "hardware/devices/lrfdevice.h"

// ============================================================================
// Qt Framework
// ============================================================================
#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

CameraController::CameraController(DayCameraControlDevice* dayControl,
                                   CameraVideoStreamDevice* dayProcessor,
                                   NightCameraControlDevice* nightControl,
                                   CameraVideoStreamDevice* nightProcessor,
                                   SystemStateModel* stateModel,
                                   LRFDevice* lrfDevice,
                                   QObject* parent)
    : QObject(parent)
    , m_dayControl(dayControl)
    , m_dayProcessor(dayProcessor)
    , m_nightControl(nightControl)
    , m_nightProcessor(nightProcessor)
    , m_stateModel(stateModel)
    , m_lrfDevice(lrfDevice)
{
    // Connect to system state changes
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &CameraController::onSystemStateChanged, Qt::QueuedConnection);

        m_isDayCameraActive = m_stateModel->data().activeCameraIsDay;
        qInfo() << "[CameraController] Initialized. Active camera:"
                << (m_isDayCameraActive ? "Day" : "Night");
    } else {
        qWarning() << "[CameraController] Created without SystemStateModel!";
    }

    // LRF data flows through: LRFDevice -> LrfDataModel -> SystemStateModel
    // CameraController only provides control interface (trigger/start/stop)
    if (m_lrfDevice) {
        qInfo() << "[CameraController] LRF device available for ranging control";
    } else {
        qWarning() << "[CameraController] LRF device not available!";
    }
}

CameraController::~CameraController()
{
    qInfo() << "[CameraController] Destructor";
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool CameraController::initialize()
{
    // CameraVideoStreamDevice instances are started by SystemController
    // CameraControlDevices are opened by SystemController

    if (!m_stateModel || !m_dayControl || !m_nightControl ||
        !m_dayProcessor || !m_nightProcessor) {
        updateStatus("Initialization failed: Missing required components.");
        return false;
    }

    updateStatus("CameraController initialized.");
    return true;
}

// ============================================================================
// CAMERA PROCESSOR ACCESSORS
// ============================================================================

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
    return m_isDayCameraActive ? m_dayProcessor : m_nightProcessor;
}

bool CameraController::isDayCameraActive() const
{
    return m_isDayCameraActive;
}

// ============================================================================
// STATE CHANGE HANDLER
// ============================================================================

void CameraController::onSystemStateChanged(const SystemStateData& newData)
{
    QMutexLocker locker(&m_mutex);

    bool cameraChanged = (m_cachedState.activeCameraIsDay != newData.activeCameraIsDay);

    // Keep previous state before updating
    SystemStateData oldStateBeforeUpdate = m_cachedState;
    m_cachedState = newData;

    // Handle camera switch
    if (cameraChanged) {
        setActiveCamera(newData.activeCameraIsDay);

        // Stop tracking on the camera that just became inactive
        CameraVideoStreamDevice* oldProcessor = oldStateBeforeUpdate.activeCameraIsDay
                                                ? m_dayProcessor
                                                : m_nightProcessor;

        if (oldProcessor && oldStateBeforeUpdate.trackingActive) {
            qInfo() << "[CameraController] Camera switched, stopping tracking on inactive processor:"
                    << oldProcessor->property("cameraIndex").toInt();

            QMetaObject::invokeMethod(oldProcessor, "setTrackingEnabled", Qt::QueuedConnection,
                                      Q_ARG(bool, false));
        }

        emit stateChanged();
    }
}

void CameraController::setActiveCamera(bool isDay)
{
    // Assumes mutex is already locked if called from onSystemStateChanged
    if (m_isDayCameraActive != isDay) {
        m_isDayCameraActive = isDay;
        qInfo() << "[CameraController] Active camera set to:"
                << (isDay ? "Day" : "Night");
    }
}

// ============================================================================
// TRACKING CONTROL
// ============================================================================

bool CameraController::startTracking()
{
    QMutexLocker locker(&m_mutex);

    if (!m_stateModel) {
        updateStatus("Cannot start tracking: SystemStateModel missing.");
        return false;
    }

    CameraVideoStreamDevice* activeProcessor = getActiveCameraProcessor();
    if (!activeProcessor) {
        updateStatus("Cannot start tracking: No active camera processor.");
        return false;
    }

    if (m_stateModel->data().trackingActive) {
        updateStatus("Tracking already active.");
        return true;
    }

    locker.unlock();

    qInfo() << "[CameraController] Requesting tracking START on processor:"
            << activeProcessor->property("cameraIndex").toInt();

    bool invokeSuccess = QMetaObject::invokeMethod(
        activeProcessor, "setTrackingEnabled", Qt::QueuedConnection,
        Q_ARG(bool, true));

    if (!invokeSuccess) {
        qWarning() << "[CameraController] Failed to invoke setTrackingEnabled(true)";
        updateStatus("Failed to send start tracking command.");
        return false;
    }

    // Update central state model after successfully invoking the command
    if (m_stateModel) {
        m_stateModel->setTrackingStarted(true);
    }

    updateStatus(QString("Tracking start requested on %1 camera.")
                 .arg(m_isDayCameraActive ? "Day" : "Night"));

    return true;
}

void CameraController::stopTracking()
{
    QMutexLocker locker(&m_mutex);

    if (!m_stateModel) {
        updateStatus("Cannot stop tracking: SystemStateModel missing.");
        return;
    }

    CameraVideoStreamDevice* activeProcessor = getActiveCameraProcessor();
    if (!activeProcessor) {
        updateStatus("Cannot stop tracking: No active camera processor.");
        return;
    }

    if (!m_stateModel->data().trackingActive) {
        updateStatus("Tracking already stopped.");
        return;
    }

    locker.unlock();

    qInfo() << "[CameraController] Requesting tracking STOP on processor:"
            << activeProcessor->property("cameraIndex").toInt();

    bool invokeSuccess = QMetaObject::invokeMethod(
        activeProcessor, "setTrackingEnabled", Qt::QueuedConnection,
        Q_ARG(bool, false));

    if (!invokeSuccess) {
        qWarning() << "[CameraController] Failed to invoke setTrackingEnabled(false)";
        updateStatus("Failed to send stop tracking command.");
        return;
    }

    // Update central state model after successfully invoking the command
    if (m_stateModel) {
        m_stateModel->setTrackingStarted(false);
    }

    updateStatus(QString("Tracking stop requested on %1 camera.")
                 .arg(m_isDayCameraActive ? "Day" : "Night"));
}

// ============================================================================
// DAY CAMERA CONTROL (ZOOM / FOCUS)
// ============================================================================

void CameraController::zoomIn()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) {
            m_dayControl->zoomIn();
        }
    } else {
        // Night camera digital zoom
        if (m_nightControl) {
            m_nightControl->setDigitalZoom(4);
        }
    }
}

void CameraController::zoomOut()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) {
            m_dayControl->zoomOut();
        }
    } else {
        // Night camera digital zoom
        if (m_nightControl) {
            m_nightControl->setDigitalZoom(0);
        }
    }
}

void CameraController::zoomStop()
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->zoomStop();
    }
    // Night camera digital zoom doesn't have a stop command
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

// ============================================================================
// NIGHT CAMERA CONTROL (THERMAL)
// ============================================================================

void CameraController::nextVideoLUT()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_lutIndex = qMin(m_lutIndex + 1, 12);
        m_nightControl->setVideoModeLUT(m_lutIndex);
    }
}

void CameraController::prevVideoLUT()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_lutIndex = qMax(m_lutIndex - 1, 0);
        m_nightControl->setVideoModeLUT(m_lutIndex);
    }
}

void CameraController::performFFC()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_nightControl->performFFC();
    }
}

// ============================================================================
// LRF CONTROL (Laser Range Finder)
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
// STATUS UPDATE
// ============================================================================

void CameraController::updateStatus(const QString& message)
{
    if (m_statusMessage != message) {
        m_statusMessage = message;
        qDebug() << "[CameraController] Status:" << message;
        emit statusUpdated(message);
    }
}
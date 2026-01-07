#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
// ============================================================================
// Qt Framework
// ============================================================================
#include <QObject>
#include <QMutex>
#include <QPointer>

// ============================================================================
// Project
// ============================================================================
#include "models/domain/systemstatemodel.h"

// ============================================================================
// Forward Declarations
// ============================================================================
class DayCameraControlDevice;
class NightCameraControlDevice;
class CameraVideoStreamDevice;
class LRFDevice;

/**
 * @brief Camera system coordinator
 *
 * This class manages day/night camera switching, tracking control,
 * and LRF (Laser Range Finder) operations. It provides:
 * - Camera control (zoom, focus, LUT selection)
 * - Tracking enable/disable on active camera
 * - LRF single-shot and continuous ranging
 * - State synchronization with SystemStateModel
 */
class CameraController : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // Constructor / Destructor
    // ============================================================================
    explicit CameraController(DayCameraControlDevice* dayControl,
                              CameraVideoStreamDevice* dayProcessor,
                              NightCameraControlDevice* nightControl,
                              CameraVideoStreamDevice* nightProcessor, SystemStateModel* stateModel,
                              LRFDevice* lrfDevice, QObject* parent = nullptr);
    ~CameraController() override;

    bool initialize();

    // ============================================================================
    // Camera Processor Accessors
    // ============================================================================
    CameraVideoStreamDevice* getDayCameraProcessor() const;
    CameraVideoStreamDevice* getNightCameraProcessor() const;
    CameraVideoStreamDevice* getActiveCameraProcessor() const;
    bool isDayCameraActive() const;

    // ============================================================================
    // Day Camera Control
    // ============================================================================
    Q_INVOKABLE virtual void zoomIn();
    Q_INVOKABLE virtual void zoomOut();
    Q_INVOKABLE virtual void zoomStop();
    Q_INVOKABLE void focusNear();
    Q_INVOKABLE void focusFar();
    Q_INVOKABLE void focusStop();
    Q_INVOKABLE void setFocusAuto(bool enabled);

    // ============================================================================
    // Night Camera Control (Thermal)
    // ============================================================================
    Q_INVOKABLE void nextVideoLUT();
    Q_INVOKABLE void prevVideoLUT();
    Q_INVOKABLE void performFFC();

    // ============================================================================
    // Tracking Control
    // ============================================================================
    Q_INVOKABLE bool startTracking();
    Q_INVOKABLE void stopTracking();

    // ============================================================================
    // LRF Control (Laser Range Finder)
    // Called by JoystickController when Button 1 is pressed
    // ============================================================================
    Q_INVOKABLE void triggerLRF();
    Q_INVOKABLE void startContinuousLRF();
    Q_INVOKABLE void stopContinuousLRF();

signals:
    // ============================================================================
    // State Notifications
    // ============================================================================
    void stateChanged();
    void statusUpdated(const QString& message);

public slots:
    // ============================================================================
    // System State Handler
    // ============================================================================
    void onSystemStateChanged(const SystemStateData& newData);

private:
    // ============================================================================
    // Private Methods
    // ============================================================================
    void updateStatus(const QString& message);
    void setActiveCamera(bool isDay);

    // ============================================================================
    // Dependencies
    // ============================================================================
    QPointer<DayCameraControlDevice> m_dayControl;
    QPointer<CameraVideoStreamDevice> m_dayProcessor;
    QPointer<NightCameraControlDevice> m_nightControl;
    QPointer<CameraVideoStreamDevice> m_nightProcessor;
    QPointer<SystemStateModel> m_stateModel;
    QPointer<LRFDevice> m_lrfDevice;

    // ============================================================================
    // Internal State
    // ============================================================================
    QMutex m_mutex;
    bool m_isDayCameraActive = true;
    SystemStateData m_cachedState;
    int m_lutIndex = 0;
    QString m_statusMessage;
};


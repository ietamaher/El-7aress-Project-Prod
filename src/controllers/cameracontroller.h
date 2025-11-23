#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QObject>
#include <QMutex>
#include <QPointer> // Use QPointer for robustness if models/devices can be deleted
#include "models/domain/systemstatedata.h"
#include "models/domain/systemstatemodel.h"
// Forward Declarations
class DayCameraControlDevice;
class NightCameraControlDevice;
class CameraVideoStreamDevice; // Replaces pipeline devices
class SystemStateModel;
class LRFDevice;

class CameraController : public QObject
{
    Q_OBJECT

public:
    explicit CameraController(DayCameraControlDevice* dayControl,
                              CameraVideoStreamDevice* dayProcessor,
                              NightCameraControlDevice* nightControl,
                              CameraVideoStreamDevice* nightProcessor,
                              SystemStateModel* stateModel,
                              LRFDevice* lrfDevice,
                              QObject* parent = nullptr);
    ~CameraController() override;

    bool initialize(); // Simplified initialization

    // --- Camera Control Methods (Remain Largely the Same) ---
    Q_INVOKABLE virtual void zoomIn();
    Q_INVOKABLE virtual void zoomOut();
    Q_INVOKABLE virtual void zoomStop();
    Q_INVOKABLE void focusNear();
    Q_INVOKABLE void focusFar();
    Q_INVOKABLE void focusStop();
    Q_INVOKABLE void setFocusAuto(bool enabled);
    // Night camera specific
    Q_INVOKABLE void nextVideoLUT();
    Q_INVOKABLE void prevVideoLUT();
    Q_INVOKABLE void performFFC();

    // --- Tracking Control ---
    Q_INVOKABLE bool startTracking(); // Request tracking on active camera
    Q_INVOKABLE void stopTracking();  // Request tracking stop on active camera

    // --- LRF Control (Laser Range Finder) ---
    Q_INVOKABLE void triggerLRF();           // Single shot ranging (Button 1)
    Q_INVOKABLE void startContinuousLRF();   // Continuous ranging (5Hz)
    Q_INVOKABLE void stopContinuousLRF();    // Stop continuous ranging

    // --- Getters ---
    CameraVideoStreamDevice* getDayCameraProcessor() const;   // Changed name/type
    CameraVideoStreamDevice* getNightCameraProcessor() const; // Changed name/type
    CameraVideoStreamDevice* getActiveCameraProcessor() const; // Changed name/type
    bool isDayCameraActive() const;

signals:
    // Simple signal indicating some relevant state might have changed
    // (e.g., active camera changed, tracking toggled)
    void stateChanged();
    // Signal to update external status displays
    void statusUpdated(const QString& message);

public slots:
    // React to changes in the central state model
    void onSystemStateChanged(std::shared_ptr<const SystemStateData> newData);

private:
    void updateStatus(const QString& message);
    void setActiveCamera(bool isDay); // Internal helper to manage state on change

    // --- Dependencies ---
    QPointer<DayCameraControlDevice>    m_dayControl;
    QPointer<CameraVideoStreamDevice>   m_dayProcessor; // Changed
    QPointer<NightCameraControlDevice>  m_nightControl;
    QPointer<CameraVideoStreamDevice>   m_nightProcessor; // Changed
    QPointer<SystemStateModel>          m_stateModel;
    QPointer<LRFDevice>                 m_lrfDevice;     // Laser Range Finder

    // --- Internal State ---
    QMutex m_mutex; // For thread safety if needed, although most action is on state model signals
    bool m_isDayCameraActive = true; // Cache the active camera flag
    SystemStateData m_cachedState;   // Cache the last known state from the model

    /*
     * /home/rapit/Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.h:81: error: field ‘m_cachedState’ has incomplete type ‘SystemStateData’
In file included from moc_cameracontroller.cpp:9:
../../Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.h:81:21: error: field ‘m_cachedState’ has incomplete type ‘SystemStateData’
   81 |     SystemStateData m_cachedState;   // Cache the last known state from the model
      |                     ^~~~~~~~~~~~~
../../Desktop/tous_dossiers/docs/el7aress/controllers/cameracontroller.h:14:7: note: forward declaration of ‘class SystemStateData’
   14 | class SystemStateData; // Assuming SystemStateModel emits this
      |       ^~~~~~~~~~~~~~~
      */

    int m_lutIndex = 0; // Keep track of LUT index for night camera
    QString statusMessage; // Last status message
};

#endif // CAMERACONTROLLER_H

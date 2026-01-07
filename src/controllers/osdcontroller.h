#ifndef OSDCONTROLLER_H
#define OSDCONTROLLER_H

#include <QObject>
#include <QTimer>

// Forward declarations
class OsdViewModel;
class SystemStateModel;
struct FrameData;
struct SystemStateData;

/**
 * @brief OsdController - Manages OSD updates from SystemStateModel
 *
 * This controller bridges SystemStateModel to OsdViewModel.
 *
 * PHASE 1 (Active NOW): Updates from SystemStateModel
 * PHASE 2 (Later): Can also update from CameraVideoStreamDevice FrameData
 */
class OsdController : public QObject {
    Q_OBJECT

public:
    explicit OsdController(QObject* parent = nullptr);

    // Dependency injection (called by SystemController)
    void setViewModel(OsdViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

    // Initialize connections
    void initialize();

    // Startup sequence control
    void startStartupSequence();
    void showErrorMessage(const QString& errorText);
    void hideErrorMessage();

public slots:
    // PHASE 1: Direct from SystemStateModel (Active NOW)
    void onSystemStateChanged(const SystemStateData& data);

    // PHASE 2: From CameraVideoStreamDevice (Uncomment when ready)
    void onFrameDataReady(const FrameData& frmdata);

private slots:
    void onColorStyleChanged(const QColor& color);
    void advanceStartupSequence();
    void onStartupSystemStateChanged(const SystemStateData& data);
    void onStaticDetectionTimerExpired();

private:
    // Startup sequence states
    enum class StartupState {
        Idle,
        SystemInit,
        WaitingForIMU,
        DetectingStatic,
        CalibratingAHRS,
        WaitingForCriticalDevices,
        SystemReady,
        Complete
    };

    void updateStartupMessage(StartupState state);
    void checkDevicesAndAdvance(const SystemStateData& data);
    bool areCriticalDevicesConnected(const SystemStateData& data) const;
    void checkForCriticalErrors(const SystemStateData& data);

    // Shared update logic
    //void updateViewModelFromSystemState(const SystemStateData& data);

    // Dependencies (injected)
    OsdViewModel* m_viewModel;
    SystemStateModel* m_stateModel;

    int m_activeCameraIndex;

    // Startup sequence state machine
    QTimer* m_startupTimer;
    QTimer* m_staticDetectionTimer;
    StartupState m_startupState;
    bool m_startupSequenceActive;

    // Device connection tracking
    bool m_imuConnected;
    bool m_staticDetectionComplete;
};

#endif  // OSDCONTROLLER_H

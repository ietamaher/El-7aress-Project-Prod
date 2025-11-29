#ifndef JOYSTICKCONTROLLER_H
#define JOYSTICKCONTROLLER_H

#include <QObject>
#include "models/domain/joystickdatamodel.h"  // for JoystickData
#include "models/domain/systemstatemodel.h"  // for SystemStateModel (m_stateModel)
#include "controllers/gimbalcontroller.h"   // optional
#include "controllers/cameracontroller.h"   // if you have it
#include "controllers/weaponcontroller.h"   // or whatever controllers you need

class JoystickController : public QObject
{
    Q_OBJECT
public:
    explicit JoystickController(JoystickDataModel *joystickModel,
                                SystemStateModel *stateModel,
                                GimbalController *gimbalCtrl,
                                CameraController *cameraCtrl,
                                WeaponController *weaponCtrl,
                                QObject *parent=nullptr);
    ~JoystickController() {}

signals:
         // You can define signals if you want the joystick to trigger events
         // e.g. void requestModeChange(OperationalMode newMode);
    void trackListUpdated(bool state);
    void trackSelectButtonPressed();
public slots:
    // Connect these slots to `JoystickDataModel::axisChanged(...)` or `dataChanged(...)`
    // or directly to `JoystickDevice::axisMoved(...)`, `buttonPressed(...)`
    void onAxisChanged(int axis, float normalizedValue);
    void onButtonChanged(int button, bool pressed);
    void onHatChanged(int hat, int value); // For hat switches (if needed)

private:
    JoystickDataModel *m_joystickModel = nullptr;
    SystemStateModel *m_stateModel = nullptr;
    GimbalController *m_gimbalController = nullptr;
    CameraController *m_cameraController = nullptr;
    WeaponController *m_weaponController = nullptr;
    // Possibly references to other controllers (camera, weapon, etc.)

    // This helps if you want to remember the last operational mode
    OperationalMode m_previousMode = OperationalMode::Idle;

    bool m_tracklistActive = false;
    // For advanced detection toggles, etc.
    bool m_detectionEnabled = false;

    int videoLUT = 0;
    int m_activeCameraIndex = 0;
    qint64 m_lastTrackButtonPressTime = 0; // For debouncing track button presses
    static constexpr int DOUBLE_CLICK_INTERVAL_MS = 1000; // 1000ms for double-click

    // ⭐ Enhanced LRF functionality (single press vs double press)
    qint64 m_lastLrfButtonPressTime = 0;  // Track LRF button press timing
    bool m_continuousLrfActive = false;    // Track continuous LRF state
};

#endif // JOYSTICKCONTROLLER_H


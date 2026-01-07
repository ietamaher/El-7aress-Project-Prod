#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>

#include "models/domain/joystickdatamodel.h"
#include "models/domain/systemstatemodel.h"
#include "controllers/gimbalcontroller.h"
#include "controllers/cameracontroller.h"
#include "controllers/weaponcontroller.h"

class JoystickController : public QObject {
    Q_OBJECT

public:
    explicit JoystickController(JoystickDataModel* joystickModel, SystemStateModel* stateModel,
                                GimbalController* gimbalCtrl, CameraController* cameraCtrl,
                                WeaponController* weaponCtrl, QObject* parent = nullptr);
    ~JoystickController() = default;

signals:
    void trackSelectButtonPressed();

public slots:
    void onAxisChanged(int axis, float normalizedValue);
    void onButtonChanged(int button, bool pressed);
    void onHatChanged(int hat, int value);

private:
    // ============================================================================
    // Constants
    // ============================================================================
    static constexpr int DOUBLE_CLICK_INTERVAL_MS = 1000;

    // ============================================================================
    // Dependencies (injected controllers and models)
    // ============================================================================
    JoystickDataModel* m_joystickModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;
    GimbalController* m_gimbalController = nullptr;
    CameraController* m_cameraController = nullptr;
    WeaponController* m_weaponController = nullptr;

    // ============================================================================
    // Camera State
    // ============================================================================
    int m_activeCameraIndex = 0;
    int m_videoLUT = 0;

    // ============================================================================
    // Track Button State (double-click detection)
    // ============================================================================
    qint64 m_lastTrackButtonPressTime = 0;

    // ============================================================================
    // LRF State
    // PDF: Single press = single measurement, Double press = toggle continuous mode
    // ============================================================================
    qint64 m_lastLrfButtonPressTime = 0;
    bool m_continuousLrfActive = false;
};


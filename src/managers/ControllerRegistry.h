#ifndef CONTROLLERREGISTRY_H
#define CONTROLLERREGISTRY_H

#include <QObject>

// Forward declarations - Hardware Controllers
class GimbalController;
class WeaponController;
class CameraController;
class JoystickController;
class LedController;

// Forward declarations - QML Controllers
class OsdController;
class ZoneDefinitionController;
class MainMenuController;
class ReticleMenuController;
class ColorMenuController;
class ZeroingController;
class WindageController;
class EnvironmentalController;
class HomeCalibrationController;
// class SystemStatusController;  // DISABLED
class AboutController;
class ApplicationController;

// Forward declarations - Managers & Models
class HardwareManager;
class ViewModelRegistry;
class SystemStateModel;
class VideoImageProvider;
class CameraVideoStreamDevice;

class QQmlContext;

/**
 * @class ControllerRegistry
 * @brief Central registry for creating and managing all Controllers.
 *
 * This class handles the creation, initialization, and interconnection
 * of both hardware controllers and QML UI controllers.
 *
 * Two types of controllers:
 * - Hardware Controllers: Manage physical devices (gimbal, weapon, camera, joystick)
 * - QML Controllers: Manage UI logic and ViewModels
 */
class ControllerRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ControllerRegistry(HardwareManager* hardwareManager,
                                ViewModelRegistry* viewModelRegistry,
                                SystemStateModel* systemStateModel,
                                QObject* parent = nullptr);
    ~ControllerRegistry();

    // ========================================================================
    // INITIALIZATION PHASES
    // ========================================================================

    /**
     * @brief Creates all hardware controllers
     * @return true if successful
     */
    bool createHardwareControllers();

    /**
     * @brief Creates all QML controllers
     * @return true if successful
     */
    bool createQmlControllers();

    /**
     * @brief Initializes and connects all controllers
     * @return true if successful
     */
    bool initializeControllers();

    /**
     * @brief Connects video processors to OSD controller for frame-sync
     * Must be called after QML controllers are created
     */
    bool connectVideoToOsd();

    /**
     * @brief Registers ApplicationController with QML
     * @param context The QML root context
     * @return true if successful
     */
    bool registerWithQml(QQmlContext* context);

    // ========================================================================
    // CONTROLLER ACCESSORS
    // ========================================================================

    // Hardware Controllers
    GimbalController* gimbalController() const { return m_gimbalController; }
    WeaponController* weaponController() const { return m_weaponController; }
    CameraController* cameraController() const { return m_cameraController; }
    JoystickController* joystickController() const { return m_joystickController; }

    // QML Controllers
    OsdController* osdController() const { return m_osdController; }
    ZoneDefinitionController* zoneDefinitionController() const { return m_zoneDefinitionController; }
    MainMenuController* mainMenuController() const { return m_mainMenuController; }
    ReticleMenuController* reticleMenuController() const { return m_reticleMenuController; }
    ColorMenuController* colorMenuController() const { return m_colorMenuController; }
    ZeroingController* zeroingController() const { return m_zeroingController; }
    WindageController* windageController() const { return m_windageController; }
    EnvironmentalController* environmentalController() const { return m_environmentalController; }
    HomeCalibrationController* homeCalibrationController() const { return m_homeCalibrationController; }
    // SystemStatusController* systemStatusController() const { return m_systemStatusController; }  // DISABLED
    AboutController* aboutController() const { return m_aboutController; }
    ApplicationController* applicationController() const { return m_appController; }
    LedController* ledController() const { return m_ledController; }

signals:
    void hardwareControllersCreated();
    void qmlControllersCreated();
    void controllersInitialized();

private:
    // ========================================================================
    // HARDWARE CONTROLLERS
    // ========================================================================
    GimbalController* m_gimbalController = nullptr;
    WeaponController* m_weaponController = nullptr;
    CameraController* m_cameraController = nullptr;
    JoystickController* m_joystickController = nullptr;

    // ========================================================================
    // QML CONTROLLERS
    // ========================================================================
    OsdController* m_osdController = nullptr;
    ZoneDefinitionController* m_zoneDefinitionController = nullptr;
    MainMenuController* m_mainMenuController = nullptr;
    ReticleMenuController* m_reticleMenuController = nullptr;
    ColorMenuController* m_colorMenuController = nullptr;
    ZeroingController* m_zeroingController = nullptr;
    WindageController* m_windageController = nullptr;
    EnvironmentalController* m_environmentalController = nullptr;
    HomeCalibrationController* m_homeCalibrationController = nullptr;
    // SystemStatusController* m_systemStatusController = nullptr;  // DISABLED
    AboutController* m_aboutController = nullptr;
    ApplicationController* m_appController = nullptr;
    LedController* m_ledController = nullptr;

    // ========================================================================
    // DEPENDENCIES (not owned)
    // ========================================================================
    HardwareManager* m_hardwareManager = nullptr;
    ViewModelRegistry* m_viewModelRegistry = nullptr;
    SystemStateModel* m_systemStateModel = nullptr;
};

#endif // CONTROLLERREGISTRY_H

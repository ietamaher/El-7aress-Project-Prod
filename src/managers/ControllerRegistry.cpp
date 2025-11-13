#include "ControllerRegistry.h"
#include "HardwareManager.h"
#include "ViewModelRegistry.h"

// Hardware Controllers
#include "controllers/gimbalcontroller.h"
#include "controllers/weaponcontroller.h"
#include "controllers/cameracontroller.h"
#include "controllers/joystickcontroller.h"
#include "controllers/ledcontroller.h"

// QML Controllers
#include "controllers/applicationcontroller.h"
#include "controllers/mainmenucontroller.h"
#include "controllers/reticlemenucontroller.h"
#include "controllers/colormenucontroller.h"
#include "controllers/zeroingcontroller.h"
#include "controllers/windagecontroller.h"
#include "controllers/environmentalcontroller.h"
#include "controllers/osdcontroller.h"
#include "controllers/zonedefinitioncontroller.h"
#include "controllers/systemstatuscontroller.h"
#include "controllers/aboutcontroller.h"

// ViewModels
#include "models/osdviewmodel.h"
#include "models/menuviewmodel.h"
#include "models/zonedefinitionviewmodel.h"
#include "models/zonemapviewmodel.h"
#include "models/areazoneparameterviewmodel.h"
#include "models/sectorscanparameterviewmodel.h"
#include "models/trpparameterviewmodel.h"
#include "models/zeroingviewmodel.h"
#include "models/windageviewmodel.h"
#include "models/systemstatusviewmodel.h"
#include "models/aboutviewmodel.h"

// Domain Models
#include "models/domain/systemstatemodel.h"

// Hardware Devices
#include "hardware/devices/cameravideostreamdevice.h"

#include <QQmlContext>
#include <QDebug>

ControllerRegistry::ControllerRegistry(HardwareManager* hardwareManager,
                                       ViewModelRegistry* viewModelRegistry,
                                       SystemStateModel* systemStateModel,
                                       QObject* parent)
    : QObject(parent),
      m_hardwareManager(hardwareManager),
      m_viewModelRegistry(viewModelRegistry),
      m_systemStateModel(systemStateModel)
{
    if (!m_hardwareManager) {
        qCritical() << "ControllerRegistry: HardwareManager is null!";
    }
    if (!m_viewModelRegistry) {
        qCritical() << "ControllerRegistry: ViewModelRegistry is null!";
    }
    if (!m_systemStateModel) {
        qCritical() << "ControllerRegistry: SystemStateModel is null!";
    }
}

ControllerRegistry::~ControllerRegistry()
{
    qInfo() << "ControllerRegistry: Destroyed";
}

// ============================================================================
// HARDWARE CONTROLLERS
// ============================================================================

bool ControllerRegistry::createHardwareControllers()
{
    if (!m_hardwareManager || !m_systemStateModel) {
        qCritical() << "Cannot create hardware controllers: missing dependencies";
        return false;
    }

    qInfo() << "=== ControllerRegistry: Creating Hardware Controllers ===";

    try {
        // Gimbal Controller
        m_gimbalController = new GimbalController(
            m_hardwareManager->servoAzDevice(),
            m_hardwareManager->servoElDevice(),
            m_hardwareManager->plc42Device(),
            m_systemStateModel,
            this
        );

        // Weapon Controller
        m_weaponController = new WeaponController(
            m_systemStateModel,
            m_hardwareManager->servoActuatorDevice(),
            m_hardwareManager->plc42Device(),
            this
        );

        // Camera Controller
        m_cameraController = new CameraController(
            m_hardwareManager->dayCameraControl(),
            m_hardwareManager->dayVideoProcessor(),
            m_hardwareManager->nightCameraControl(),
            m_hardwareManager->nightVideoProcessor(),
            m_systemStateModel,
            this
        );

        // Joystick Controller
        m_joystickController = new JoystickController(
            m_hardwareManager->joystickDataModel(),
            m_systemStateModel,
            m_gimbalController,
            m_cameraController,
            m_weaponController,
            this
        );

        m_ledController = new LedController(
            m_systemStateModel,
            m_hardwareManager->plc21Device(),
            this
        );

        qInfo() << "  ✓ Hardware controllers created";
        emit hardwareControllersCreated();
        return true;

    } catch (const std::exception& e) {
        qCritical() << "Failed to create hardware controllers:" << e.what();
        return false;
    }
}

// ============================================================================
// QML CONTROLLERS
// ============================================================================

bool ControllerRegistry::createQmlControllers()
{
    if (!m_viewModelRegistry || !m_systemStateModel) {
        qCritical() << "Cannot create QML controllers: missing dependencies";
        return false;
    }

    qInfo() << "=== ControllerRegistry: Creating QML Controllers ===";

    try {
        // OSD Controller
        m_osdController = new OsdController(this);
        m_osdController->setViewModel(m_viewModelRegistry->osdViewModel());
        m_osdController->setStateModel(m_systemStateModel);

        // Main Menu Controller
        m_mainMenuController = new MainMenuController(this);
        m_mainMenuController->setViewModel(m_viewModelRegistry->mainMenuViewModel());
        m_mainMenuController->setStateModel(m_systemStateModel);

        // Reticle Menu Controller
        m_reticleMenuController = new ReticleMenuController(this);
        m_reticleMenuController->setViewModel(m_viewModelRegistry->reticleMenuViewModel());
        m_reticleMenuController->setOsdViewModel(m_viewModelRegistry->osdViewModel());
        m_reticleMenuController->setStateModel(m_systemStateModel);

        // Color Menu Controller
        m_colorMenuController = new ColorMenuController(this);
        m_colorMenuController->setViewModel(m_viewModelRegistry->colorMenuViewModel());
        m_colorMenuController->setOsdViewModel(m_viewModelRegistry->osdViewModel());
        m_colorMenuController->setStateModel(m_systemStateModel);

        // Zeroing Controller
        m_zeroingController = new ZeroingController(this);
        m_zeroingController->setViewModel(m_viewModelRegistry->zeroingViewModel());
        m_zeroingController->setStateModel(m_systemStateModel);

        // Windage Controller
        m_windageController = new WindageController(this);
        m_windageController->setViewModel(m_viewModelRegistry->windageViewModel());
        m_windageController->setStateModel(m_systemStateModel);

        // Environmental Controller
        m_environmentalController = new EnvironmentalController(this);
        m_environmentalController->setViewModel(m_viewModelRegistry->environmentalViewModel());
        m_environmentalController->setStateModel(m_systemStateModel);

        // Zone Definition Controller
        m_zoneDefinitionController = new ZoneDefinitionController(this);
        m_zoneDefinitionController->setViewModel(m_viewModelRegistry->zoneDefinitionViewModel());
        m_zoneDefinitionController->setMapViewModel(m_viewModelRegistry->zoneMapViewModel());
        m_zoneDefinitionController->setParameterViewModels(
            m_viewModelRegistry->areaZoneParameterViewModel(),
            m_viewModelRegistry->sectorScanParameterViewModel(),
            m_viewModelRegistry->trpParameterViewModel()
        );
        m_zoneDefinitionController->setStateModel(m_systemStateModel);

        // System Status Controller
        m_systemStatusController = new SystemStatusController();
        m_systemStatusController->setViewModel(m_viewModelRegistry->systemStatusViewModel());
        m_systemStatusController->setStateModel(m_systemStateModel);

        // About Controller
        m_aboutController = new AboutController();
        m_aboutController->setViewModel(m_viewModelRegistry->aboutViewModel());
        m_aboutController->setStateModel(m_systemStateModel);

        // Application Controller (LAST - needs all other controllers)
        m_appController = new ApplicationController(this);
        m_appController->setMainMenuController(m_mainMenuController);
        m_appController->setReticleMenuController(m_reticleMenuController);
        m_appController->setColorMenuController(m_colorMenuController);
        m_appController->setZeroingController(m_zeroingController);
        m_appController->setWindageController(m_windageController);
        m_appController->setEnvironmentalController(m_environmentalController);
        m_appController->setZoneDefinitionController(m_zoneDefinitionController);
        m_appController->setSystemStatusController(m_systemStatusController);
        m_appController->setAboutController(m_aboutController);
        m_appController->setSystemStateModel(m_systemStateModel);

        qInfo() << "  ✓ QML controllers created";
        emit qmlControllersCreated();
        return true;

    } catch (const std::exception& e) {
        qCritical() << "Failed to create QML controllers:" << e.what();
        return false;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool ControllerRegistry::initializeControllers()
{
    qInfo() << "=== ControllerRegistry: Initializing Controllers ===";

    try {
        // Initialize QML Controllers
        m_osdController->initialize();
        m_mainMenuController->initialize();
        m_reticleMenuController->initialize();
        m_colorMenuController->initialize();
        m_zeroingController->initialize();
        m_windageController->initialize();
        m_environmentalController->initialize();
        m_zoneDefinitionController->initialize();
        m_systemStatusController->initialize();
        m_aboutController->initialize();

        // Initialize ApplicationController LAST (it connects to all others)
        m_appController->initialize();

        qInfo() << "  ✓ All controllers initialized";
        emit controllersInitialized();
        return true;

    } catch (const std::exception& e) {
        qCritical() << "Failed to initialize controllers:" << e.what();
        return false;
    }
}

// ============================================================================
// VIDEO TO OSD CONNECTION
// ============================================================================

bool ControllerRegistry::connectVideoToOsd()
{
    if (!m_osdController || !m_hardwareManager) {
        qCritical() << "Cannot connect video to OSD: missing dependencies";
        return false;
    }

    qInfo() << "=== ControllerRegistry: Connecting Video to OSD ===";

    // Connect day camera
    if (m_hardwareManager->dayVideoProcessor()) {
        connect(m_hardwareManager->dayVideoProcessor(), &CameraVideoStreamDevice::frameDataReady,
                m_osdController, &OsdController::onFrameDataReady);
        qInfo() << "  ✓ Day camera → OSD controller connected";
    } else {
        qWarning() << "  ⚠ Day camera not available for OSD connection";
    }

    // Connect night camera
    if (m_hardwareManager->nightVideoProcessor()) {
        connect(m_hardwareManager->nightVideoProcessor(), &CameraVideoStreamDevice::frameDataReady,
                m_osdController, &OsdController::onFrameDataReady);
        qInfo() << "  ✓ Night camera → OSD controller connected";
    } else {
        qWarning() << "  ⚠ Night camera not available for OSD connection";
    }

    qInfo() << "  ✓ Video-to-OSD connection complete";
    return true;
}

// ============================================================================
// QML REGISTRATION
// ============================================================================

bool ControllerRegistry::registerWithQml(QQmlContext* context)
{
    if (!context) {
        qCritical() << "ControllerRegistry: QML context is null!";
        return false;
    }

    if (!m_appController) {
        qCritical() << "ControllerRegistry: ApplicationController not created!";
        return false;
    }

    qInfo() << "=== ControllerRegistry: Registering with QML ===";

    // Register ApplicationController (main entry point for QML)
    context->setContextProperty("appController", m_appController);

    // Also register SystemStateModel for debugging/direct access
    context->setContextProperty("systemStateModel", m_systemStateModel);

    qInfo() << "  ✓ Controllers registered with QML context";
    return true;
}

#include "controllers/applicationcontroller.h"
#include "controllers/mainmenucontroller.h"
#include "controllers/reticlemenucontroller.h"
#include "controllers/colormenucontroller.h"
#include "controllers/zeroingcontroller.h"
#include "controllers/windagecontroller.h"
#include "controllers/environmentalcontroller.h"
#include "controllers/brightnesscontroller.h"
#include "controllers/presethomepositioncontroller.h"
#include "controllers/zonedefinitioncontroller.h"
#include "controllers/radartargetlistcontroller.h"
// #include "controllers/systemstatuscontroller.h"  // DISABLED
#include "controllers/aboutcontroller.h"
#include "controllers/shutdownconfirmationcontroller.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>

ApplicationController::ApplicationController(QObject *parent)
    : QObject(parent)
    , m_currentMenuState(MenuState::None)
    , m_mainMenuController(nullptr)
    , m_reticleMenuController(nullptr)
    , m_colorMenuController(nullptr)
    , m_zeroingController(nullptr)
    , m_windageController(nullptr)
    , m_environmentalController(nullptr)
    , m_brightnessController(nullptr)
    , m_presetHomePositionController(nullptr)
    , m_zoneDefinitionController(nullptr)
    , m_radarTargetListController(nullptr)
    , m_systemStateModel(nullptr)
    , m_aboutController(nullptr)
    , m_shutdownConfirmationController(nullptr)
{
}

// ============================================================================
// DEPENDENCY INJECTION
// ============================================================================

void ApplicationController::setMainMenuController(MainMenuController* controller)
{
    m_mainMenuController = controller;
}

void ApplicationController::setReticleMenuController(ReticleMenuController* controller)
{
    m_reticleMenuController = controller;
}

void ApplicationController::setColorMenuController(ColorMenuController* controller)
{
    m_colorMenuController = controller;
}

void ApplicationController::setZeroingController(ZeroingController* controller)
{
    m_zeroingController = controller;
}

void ApplicationController::setWindageController(WindageController* controller)
{
    m_windageController = controller;
}

void ApplicationController::setEnvironmentalController(EnvironmentalController* controller)
{
    m_environmentalController = controller;
}

void ApplicationController::setBrightnessController(BrightnessController* controller)
{
    m_brightnessController = controller;
}

void ApplicationController::setPresetHomePositionController(PresetHomePositionController* controller)
{
    m_presetHomePositionController = controller;
}

void ApplicationController::setZoneDefinitionController(ZoneDefinitionController* controller)
{
    m_zoneDefinitionController = controller;
}

void ApplicationController::setRadarTargetListController(RadarTargetListController* controller)
{
    m_radarTargetListController = controller;
}

// void ApplicationController::setSystemStatusController(SystemStatusController* controller)  // DISABLED
// {  // DISABLED
//     m_systemStatusController = controller;  // DISABLED
// }  // DISABLED

void ApplicationController::setAboutController(AboutController* controller)
{
    m_aboutController = controller;
}

void ApplicationController::setShutdownConfirmationController(ShutdownConfirmationController* controller)
{
    m_shutdownConfirmationController = controller;
}

void ApplicationController::setSystemStateModel(SystemStateModel* model)
{
    m_systemStateModel = model;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ApplicationController::initialize()
{
    qDebug() << "ApplicationController: Initializing...";

    // Verify all dependencies are set
    Q_ASSERT(m_mainMenuController);
    Q_ASSERT(m_reticleMenuController);
    Q_ASSERT(m_colorMenuController);
    Q_ASSERT(m_zeroingController);
    Q_ASSERT(m_windageController);
    Q_ASSERT(m_environmentalController);
    Q_ASSERT(m_brightnessController);
    Q_ASSERT(m_presetHomePositionController);
    Q_ASSERT(m_zoneDefinitionController);
    Q_ASSERT(m_radarTargetListController);
    // Q_ASSERT(m_systemStatusController);  // DISABLED
    Q_ASSERT(m_aboutController);
    Q_ASSERT(m_shutdownConfirmationController);
    Q_ASSERT(m_systemStateModel);

    // =========================================================================
    // MAIN MENU CONNECTIONS
    // =========================================================================
    connect(m_mainMenuController, &MainMenuController::personalizeReticleRequested,
            this, &ApplicationController::handlePersonalizeReticle);
    connect(m_mainMenuController, &MainMenuController::personalizeColorsRequested,
            this, &ApplicationController::handlePersonalizeColors);
    connect(m_mainMenuController, &MainMenuController::brightnessRequested,
            this, &ApplicationController::handleBrightness);
    connect(m_mainMenuController, &MainMenuController::zeroingRequested,
            this, &ApplicationController::handleZeroing);
    connect(m_mainMenuController, &MainMenuController::clearZeroRequested,
            this, &ApplicationController::handleClearZero);
    connect(m_mainMenuController, &MainMenuController::windageRequested,
            this, &ApplicationController::handleWindage);
    connect(m_mainMenuController, &MainMenuController::clearWindageRequested,
            this, &ApplicationController::handleClearWindage);
    connect(m_mainMenuController, &MainMenuController::environmentalRequested,
            this, &ApplicationController::handleEnvironmental);
    connect(m_mainMenuController, &MainMenuController::clearEnvironmentalRequested,
            this, &ApplicationController::handleClearEnvironmental);
    connect(m_mainMenuController, &MainMenuController::presetHomePositionRequested,
            this, &ApplicationController::handlePresetHomePosition);
    connect(m_mainMenuController, &MainMenuController::zoneDefinitionsRequested,
            this, &ApplicationController::handleZoneDefinitions);
    // connect(m_mainMenuController, &MainMenuController::systemStatusRequested,  // DISABLED
    //         this, &ApplicationController::handleSystemStatus);  // DISABLED
    connect(m_mainMenuController, &MainMenuController::toggleDetectionRequested,
            this, &ApplicationController::handleToggleDetection);
    connect(m_mainMenuController, &MainMenuController::shutdownSystemRequested,
            this, &ApplicationController::handleShutdown);
    connect(m_mainMenuController, &MainMenuController::radarTargetListRequested,
            this, &ApplicationController::handleRadarTargetList);
    connect(m_mainMenuController, &MainMenuController::helpAboutRequested,
            this, &ApplicationController::handleHelpAbout);
    connect(m_mainMenuController, &MainMenuController::menuFinished,
            this, &ApplicationController::handleMainMenuFinished);

    // =========================================================================
    // RETICLE MENU CONNECTIONS
    // =========================================================================
    connect(m_reticleMenuController, &ReticleMenuController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_reticleMenuController, &ReticleMenuController::menuFinished,
            this, &ApplicationController::handleReticleMenuFinished);

    // =========================================================================
    // COLOR MENU CONNECTIONS
    // =========================================================================
    connect(m_colorMenuController, &ColorMenuController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_colorMenuController, &ColorMenuController::menuFinished,
            this, &ApplicationController::handleColorMenuFinished);

    // =========================================================================
    // ZEROING CONNECTIONS
    // =========================================================================
    connect(m_zeroingController, &ZeroingController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_zeroingController, &ZeroingController::zeroingFinished,
            this, &ApplicationController::handleZeroingFinished);

    // =========================================================================
    // WINDAGE CONNECTIONS
    // =========================================================================
    connect(m_windageController, &WindageController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_windageController, &WindageController::windageFinished,
            this, &ApplicationController::handleWindageFinished);

    // =========================================================================
    // ENVIRONMENTAL CONNECTIONS
    // =========================================================================
    connect(m_environmentalController, &EnvironmentalController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_environmentalController, &EnvironmentalController::environmentalFinished,
            this, &ApplicationController::handleEnvironmentalFinished);

    // =========================================================================
    // BRIGHTNESS CONNECTIONS
    // =========================================================================
    connect(m_brightnessController, &BrightnessController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_brightnessController, &BrightnessController::brightnessFinished,
            this, &ApplicationController::handleBrightnessFinished);

    // =========================================================================
    // PRESET HOME POSITION CONNECTIONS
    // =========================================================================
    connect(m_presetHomePositionController, &PresetHomePositionController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_presetHomePositionController, &PresetHomePositionController::procedureFinished,
            this, &ApplicationController::handlePresetHomePositionFinished);

    // =========================================================================
    // ZONE DEFINITION CONNECTIONS
    // =========================================================================
    connect(m_zoneDefinitionController, &ZoneDefinitionController::returnToMainMenu,
            this, &ApplicationController::handleReturnToMainMenu);
    connect(m_zoneDefinitionController, &ZoneDefinitionController::closed,
            this, &ApplicationController::handleZoneDefinitionFinished);

    // ========================================================================
    // CONNECT SYSTEM STATUS CONTROLLER
    // ========================================================================
    // if (m_systemStatusController) {  // DISABLED
    //     connect(m_systemStatusController, &SystemStatusController::menuFinished,  // DISABLED
    //             this, &ApplicationController::handleSystemStatusFinished);  // DISABLED
    //     connect(m_systemStatusController, &SystemStatusController::returnToMainMenu,  // DISABLED
    //             this, &ApplicationController::handleReturnToMainMenu);  // DISABLED
    //     qDebug() << "ApplicationController: SystemStatusController signals connected";  // DISABLED
    // }  // DISABLED

    // ========================================================================
    // CONNECT ABOUT CONTROLLER
    // ========================================================================
    if (m_aboutController) {
        connect(m_aboutController, &AboutController::aboutFinished,
                this, &ApplicationController::handleAboutFinished);
        connect(m_aboutController, &AboutController::returnToMainMenu,
                this, &ApplicationController::handleReturnToMainMenu);
        qDebug() << "ApplicationController: AboutController signals connected";
    }

    // ========================================================================
    // CONNECT SHUTDOWN CONFIRMATION CONTROLLER
    // ========================================================================
    if (m_shutdownConfirmationController) {
        connect(m_shutdownConfirmationController, &ShutdownConfirmationController::dialogFinished,
                this, &ApplicationController::handleShutdownConfirmationFinished);
        connect(m_shutdownConfirmationController, &ShutdownConfirmationController::returnToMainMenu,
                this, &ApplicationController::handleReturnToMainMenu);
        qDebug() << "ApplicationController: ShutdownConfirmationController signals connected";
    }

    // ========================================================================
    // CONNECT RADAR TARGET LIST CONTROLLER
    // ========================================================================
    if (m_radarTargetListController) {
        connect(m_radarTargetListController, &RadarTargetListController::listClosed,
                this, &ApplicationController::handleRadarTargetListFinished);
        connect(m_radarTargetListController, &RadarTargetListController::returnToMainMenu,
                this, &ApplicationController::handleReturnToMainMenu);
        qDebug() << "ApplicationController: RadarTargetListController signals connected";
    }

    // =========================================================================
    // HARDWARE BUTTON MONITORING (PLC21 switches)
    // =========================================================================
    // ✅ LATENCY FIX: Use dedicated buttonStateChanged signal instead of dataChanged
    // Reduces event load from 1,200/min (20Hz) to ~10-20/min (only on actual button presses)
    connect(m_systemStateModel, &SystemStateModel::buttonStateChanged,
            this, &ApplicationController::onButtonStateChanged,
            Qt::QueuedConnection);  // Non-blocking signal delivery
    qDebug() << "ApplicationController: PLC21 button monitoring connected (optimized)";

    qDebug() << "ApplicationController: All signal connections established";
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void ApplicationController::setMenuState(MenuState state)
{
    m_currentMenuState = state;
    qDebug() << "ApplicationController: Menu state changed to" << static_cast<int>(state);
}

void ApplicationController::showMainMenu()
{
    qDebug() << "ApplicationController: showMainMenu() called";
    hideAllMenus();
    m_mainMenuController->show();
    setMenuState(MenuState::MainMenu);
}

void ApplicationController::hideAllMenus()
{
    m_mainMenuController->hide();
    m_reticleMenuController->hide();
    m_colorMenuController->hide();
    m_zeroingController->hide();
    m_windageController->hide();
    m_environmentalController->hide();
    m_brightnessController->hide();
    m_presetHomePositionController->hide();
    m_zoneDefinitionController->hide();
    // m_systemStatusController->hide();  // DISABLED
    m_aboutController->hide();
    m_shutdownConfirmationController->hide();
    if (m_radarTargetListController) m_radarTargetListController->hide();
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void ApplicationController::onMenuValButtonPressed()
{
    qDebug() << "ApplicationController: MENU/VAL button pressed in state"
             << static_cast<int>(m_currentMenuState);

    // Handle procedures first (they have priority)
    if (m_currentMenuState == MenuState::ZeroingProcedure ||
        m_currentMenuState == MenuState::WindageProcedure ||
        m_currentMenuState == MenuState::EnvironmentalProcedure ||
        m_currentMenuState == MenuState::BrightnessProcedure ||
        m_currentMenuState == MenuState::PresetHomePositionProcedure ||
        m_currentMenuState == MenuState::ZoneDefinition     ||
        m_currentMenuState == MenuState::HelpAbout ||
        m_currentMenuState == MenuState::ShutdownConfirmation ||
        m_currentMenuState == MenuState::RadarTargets ||
        // m_currentMenuState == MenuState::SystemStatus  ||  // DISABLED
        false) {
        handleMenuValInProcedure();
        return;
    }

    // Handle menu states
    if (m_currentMenuState == MenuState::None) {
        showMainMenu();
        return;
    }

    if (m_currentMenuState == MenuState::MainMenu) {
        m_mainMenuController->onSelectButtonPressed();
        return;
    }

    if (m_currentMenuState == MenuState::ReticleMenu) {
        m_reticleMenuController->onSelectButtonPressed();
        return;
    }

    if (m_currentMenuState == MenuState::ColorMenu) {
        m_colorMenuController->onSelectButtonPressed();
        return;
    }



    qWarning() << "ApplicationController: MENU/VAL pressed in unhandled state:"
               << static_cast<int>(m_currentMenuState);
}

void ApplicationController::handleMenuValInNoMenuState()
{
    qDebug() << "ApplicationController: Opening main menu";
    showMainMenu();
}

void ApplicationController::handleMenuValInMainMenu()
{
    qDebug() << "ApplicationController: Selecting main menu item";
    m_mainMenuController->onSelectButtonPressed();
}

void ApplicationController::handleMenuValInSubmenu()
{
    qDebug() << "ApplicationController: Selecting submenu item";

    switch (m_currentMenuState) {
    case MenuState::ReticleMenu:
        m_reticleMenuController->onSelectButtonPressed();
        break;
    case MenuState::ColorMenu:
        m_colorMenuController->onSelectButtonPressed();
        break;
    default:
        break;
    }
}

void ApplicationController::handleMenuValInProcedure()
{
    qDebug() << "ApplicationController: Confirming procedure step";

    switch (m_currentMenuState) {
    case MenuState::ZeroingProcedure:
        m_zeroingController->onSelectButtonPressed();
        break;
    case MenuState::WindageProcedure:
        m_windageController->onSelectButtonPressed();
        break;
    case MenuState::EnvironmentalProcedure:
        m_environmentalController->onSelectButtonPressed();
        break;
    case MenuState::BrightnessProcedure:
        m_brightnessController->onSelectButtonPressed();
        break;
    case MenuState::PresetHomePositionProcedure:
        m_presetHomePositionController->onSelectButtonPressed();
        break;
    case MenuState::ZoneDefinition:
        m_zoneDefinitionController->onMenuValButtonPressed();
        break;
    // case MenuState::SystemStatus:  // DISABLED
    //     if (m_systemStatusController) m_systemStatusController->onSelectButtonPressed();  // DISABLED
    //     break;  // DISABLED
    case MenuState::HelpAbout:
        if (m_aboutController) m_aboutController->onSelectButtonPressed();
        break;
    case MenuState::ShutdownConfirmation:
        if (m_shutdownConfirmationController) m_shutdownConfirmationController->onSelectButtonPressed();
        break;
    case MenuState::RadarTargets:
        if (m_radarTargetListController) m_radarTargetListController->onSelectButtonPressed();
        break;
    default:
        break;
    }
}

void ApplicationController::onUpButtonPressed()
{
    qDebug() << "ApplicationController: UP button pressed";

    switch (m_currentMenuState) {
    case MenuState::MainMenu:
        m_mainMenuController->onUpButtonPressed();
        break;
    case MenuState::ReticleMenu:
        m_reticleMenuController->onUpButtonPressed();
        break;
    case MenuState::ColorMenu:
        m_colorMenuController->onUpButtonPressed();
        break;
    case MenuState::ZeroingProcedure:
        m_zeroingController->onUpButtonPressed();
        break;
    case MenuState::WindageProcedure:
        m_windageController->onUpButtonPressed();
        break;
    case MenuState::EnvironmentalProcedure:
        m_environmentalController->onUpButtonPressed();
        break;
    case MenuState::BrightnessProcedure:
        m_brightnessController->onUpButtonPressed();
        break;
    case MenuState::PresetHomePositionProcedure:
        m_presetHomePositionController->onUpButtonPressed();
        break;
    case MenuState::ZoneDefinition:
        m_zoneDefinitionController->onUpButtonPressed();
        break;
    // case MenuState::SystemStatus:  // DISABLED
    //     if (m_systemStatusController) m_systemStatusController->onUpButtonPressed();  // DISABLED
    //     break;  // DISABLED
    case MenuState::HelpAbout:
        if (m_aboutController) m_aboutController->onUpButtonPressed();
        break;
    case MenuState::ShutdownConfirmation:
        if (m_shutdownConfirmationController) m_shutdownConfirmationController->onUpButtonPressed();
        break;
    case MenuState::RadarTargets:
        if (m_radarTargetListController) m_radarTargetListController->onUpButtonPressed();
        break;

    default:
        qDebug() << "ApplicationController: UP pressed with no active menu";
        break;
    }
}

void ApplicationController::onDownButtonPressed()
{
    qDebug() << "ApplicationController: DOWN button pressed";

    switch (m_currentMenuState) {
    case MenuState::MainMenu:
        m_mainMenuController->onDownButtonPressed();
        break;
    case MenuState::ReticleMenu:
        m_reticleMenuController->onDownButtonPressed();
        break;
    case MenuState::ColorMenu:
        m_colorMenuController->onDownButtonPressed();
        break;
    case MenuState::ZeroingProcedure:
        m_zeroingController->onDownButtonPressed();
        break;
    case MenuState::WindageProcedure:
        m_windageController->onDownButtonPressed();
        break;
    case MenuState::EnvironmentalProcedure:
        m_environmentalController->onDownButtonPressed();
        break;
    case MenuState::BrightnessProcedure:
        m_brightnessController->onDownButtonPressed();
        break;
    case MenuState::PresetHomePositionProcedure:
        m_presetHomePositionController->onDownButtonPressed();
        break;
    case MenuState::ZoneDefinition:
        m_zoneDefinitionController->onDownButtonPressed();
        break;
    // case MenuState::SystemStatus:  // DISABLED
    //     if (m_systemStatusController) m_systemStatusController->onDownButtonPressed();  // DISABLED
    //     break;  // DISABLED
    case MenuState::HelpAbout:
        if (m_aboutController) m_aboutController->onDownButtonPressed();
        break;
    case MenuState::ShutdownConfirmation:
        if (m_shutdownConfirmationController) m_shutdownConfirmationController->onDownButtonPressed();
        break;
    case MenuState::RadarTargets:
        if (m_radarTargetListController) m_radarTargetListController->onDownButtonPressed();
        break;
    default:
        qDebug() << "ApplicationController: DOWN pressed with no active menu";
        break;
    }
}

// ============================================================================
// MAIN MENU ACTION HANDLERS
// ============================================================================

void ApplicationController::handlePersonalizeReticle()
{
    qDebug() << "ApplicationController: Showing Reticle Menu";
    hideAllMenus();
    m_reticleMenuController->show();
    setMenuState(MenuState::ReticleMenu);
}

void ApplicationController::handlePersonalizeColors()
{
    qDebug() << "ApplicationController: Showing Color Menu";
    hideAllMenus();
    m_colorMenuController->show();
    setMenuState(MenuState::ColorMenu);
}

void ApplicationController::handleZeroing()
{
    qDebug() << "ApplicationController: Zeroing requested";
    hideAllMenus();
    m_zeroingController->show();
    setMenuState(MenuState::ZeroingProcedure);
}

void ApplicationController::handleClearZero()
{
    qDebug() << "ApplicationController: Clear Zero requested";
    if (m_systemStateModel) {
        m_systemStateModel->clearZeroing();
    }
    showMainMenu();
}

void ApplicationController::handleWindage()
{
    qDebug() << "ApplicationController: Windage requested";
    hideAllMenus();
    m_windageController->show();
    setMenuState(MenuState::WindageProcedure);
}

void ApplicationController::handleClearWindage()
{
    qDebug() << "ApplicationController: Clear Windage requested";
    if (m_systemStateModel) {
        m_systemStateModel->clearWindage();
    }
    showMainMenu();
}

void ApplicationController::handleEnvironmental()
{
    qDebug() << "ApplicationController: Environmental settings requested";
    hideAllMenus();
    m_environmentalController->show();
    setMenuState(MenuState::EnvironmentalProcedure);
}

void ApplicationController::handleClearEnvironmental()
{
    qDebug() << "ApplicationController: Clear Environmental settings requested";
    if (m_systemStateModel) {
        m_systemStateModel->clearEnvironmental();
    }
    showMainMenu();
}

void ApplicationController::handleBrightness()
{
    qDebug() << "ApplicationController: Brightness settings requested";
    hideAllMenus();
    m_brightnessController->show();
    setMenuState(MenuState::BrightnessProcedure);
}

void ApplicationController::handlePresetHomePosition()
{
    qDebug() << "ApplicationController: Preset Home Position requested";
    hideAllMenus();
    m_presetHomePositionController->show();
    setMenuState(MenuState::PresetHomePositionProcedure);
}

void ApplicationController::handleZoneDefinitions()
{
    qDebug() << "ApplicationController: Zone Definitions requested";
    hideAllMenus();
    m_zoneDefinitionController->show();
    setMenuState(MenuState::ZoneDefinition);
}

// void ApplicationController::handleSystemStatus()  // DISABLED
// {  // DISABLED
//     qDebug() << "ApplicationController: System Status requested";  // DISABLED
//     hideAllMenus();  // DISABLED
//     m_systemStatusController->show();  // DISABLED
//     setMenuState(MenuState::SystemStatus);  // DISABLED
// }  // DISABLED

void ApplicationController::handleToggleDetection()
{
    qDebug() << "ApplicationController: Toggle Detection requested";

    if (m_systemStateModel) {
        const auto& data = m_systemStateModel->data();

        // Safety check: Only allow toggling if day camera is active
        if (!data.activeCameraIsDay) {
            qWarning() << "Cannot toggle detection - Night camera is active!";
            hideAllMenus();
            setMenuState(MenuState::None);
            return;
        }

        // Toggle detection state
        bool newState = !data.detectionEnabled;
        m_systemStateModel->setDetectionEnabled(newState);

        qInfo() << "Detection toggled to:" << (newState ? "ENABLED" : "DISABLED");
    }

    hideAllMenus();
    setMenuState(MenuState::None);
}

void ApplicationController::handleShutdown()
{
    qDebug() << "ApplicationController: Shutdown System requested - showing confirmation";
    hideAllMenus();

    // Show shutdown confirmation dialog instead of direct shutdown
    m_shutdownConfirmationController->show();
    setMenuState(MenuState::ShutdownConfirmation);
}

void ApplicationController::handleRadarTargetList()
{
    qDebug() << "ApplicationController: Radar Target List requested";
    hideAllMenus();
    if (m_radarTargetListController) {
        m_radarTargetListController->show();
    }
    setMenuState(MenuState::RadarTargets);
}

void ApplicationController::handleHelpAbout()
{
    qDebug() << "ApplicationController: Help/About requested";
    hideAllMenus();
    m_aboutController->show();
    setMenuState(MenuState::HelpAbout);
}

// ============================================================================
// COMPLETION HANDLERS
// ============================================================================

void ApplicationController::handleMainMenuFinished()
{
    qDebug() << "ApplicationController: handleMainMenuFinished()";
    qDebug() << "  Current state:" << static_cast<int>(m_currentMenuState);

    // Only close menu if still in MainMenu state
    // If state changed, an action was already taken
    if (m_currentMenuState == MenuState::MainMenu) {
        qDebug() << "  'Return ...' was selected - closing menu";
        hideAllMenus();
        setMenuState(MenuState::None);
    } else {
        qDebug() << "  State already changed, action was taken";
    }
}

void ApplicationController::handleReticleMenuFinished()
{
    qDebug() << "ApplicationController: Reticle menu finished";
}

void ApplicationController::handleColorMenuFinished()
{
    qDebug() << "ApplicationController: Color menu finished";
}

void ApplicationController::handleZeroingFinished()
{
    qDebug() << "ApplicationController: Zeroing procedure finished";
}

void ApplicationController::handleWindageFinished()
{
    qDebug() << "ApplicationController: Windage procedure finished";
}

void ApplicationController::handleEnvironmentalFinished()
{
    qDebug() << "ApplicationController: Environmental procedure finished";
}

void ApplicationController::handleBrightnessFinished()
{
    qDebug() << "ApplicationController: Brightness procedure finished";
}

void ApplicationController::handlePresetHomePositionFinished()
{
    qDebug() << "ApplicationController: Preset Home Position procedure finished";
}

void ApplicationController::handleZoneDefinitionFinished()
{
    qDebug() << "ApplicationController: Zone Definition finished";
}

void ApplicationController::handleRadarTargetListFinished()
{
    qDebug() << "ApplicationController: Radar Target List finished";
    hideAllMenus();
    setMenuState(MenuState::None);
}

// void ApplicationController::handleSystemStatusFinished()  // DISABLED
// {  // DISABLED
//     qDebug() << "ApplicationController: System Status finished";  // DISABLED
// }  // DISABLED

void ApplicationController::handleAboutFinished()
{
    qDebug() << "ApplicationController: About finished";
}

void ApplicationController::handleShutdownConfirmationFinished()
{
    qDebug() << "ApplicationController: Shutdown Confirmation finished";
}

void ApplicationController::handleReturnToMainMenu()
{
    qDebug() << "ApplicationController: handleReturnToMainMenu()";
    qDebug() << "  Current state:" << static_cast<int>(m_currentMenuState);

    hideAllMenus();
    showMainMenu();

    qDebug() << "  New state:" << static_cast<int>(m_currentMenuState);
}

// ============================================================================
// HARDWARE BUTTON MONITORING (PLC21 Panel)
// ============================================================================

void ApplicationController::onButtonStateChanged(bool menuUp, bool menuDown, bool menuVal)
{
    // ✅ LATENCY FIX: This slot is now called ONLY when buttons change, not every 50ms!
    // Event load reduced from 1,200 events/min → ~10-20 events/min (95% reduction)

    // Rising edge detection for menuUp button (false → true = button press)
    if (menuUp && !m_previousMenuUpState) {
        m_previousMenuUpState = true; // Update BEFORE calling handler to prevent re-entrancy
        onUpButtonPressed();
    } else {
        m_previousMenuUpState = menuUp;
    }

    // Rising edge detection for menuDown button (false → true = button press)
    if (menuDown && !m_previousMenuDownState) {
        m_previousMenuDownState = true; // Update BEFORE calling handler to prevent re-entrancy
        onDownButtonPressed();
    } else {
        m_previousMenuDownState = menuDown;
    }

    // Rising edge detection for menuVal button (false → true = button press)
    if (menuVal && !m_previousMenuValState) {
        m_previousMenuValState = true; // Update BEFORE calling handler to prevent re-entrancy
        onMenuValButtonPressed();
    } else {
        m_previousMenuValState = menuVal;
    }
}

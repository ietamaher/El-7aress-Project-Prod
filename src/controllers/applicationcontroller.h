#ifndef APPLICATIONCONTROLLER_H
#define APPLICATIONCONTROLLER_H

#include <QObject>
#include "models/domain/systemstatemodel.h"

// Forward declarations
class MainMenuController;
class ReticleMenuController;
class ColorMenuController;
class ZeroingController;
class WindageController;
class EnvironmentalController;
class ZoneDefinitionController;
class SystemStatusController;
class AboutController;
class SystemStateModel;

/**
 * @brief ApplicationController - Central orchestrator for all menu controllers
 *
 * This controller manages the lifecycle and transitions between different
 * menu screens and procedures in the application.
 */
class ApplicationController : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationController(QObject *parent = nullptr);

    // Dependency injection - called by SystemController
    void setMainMenuController(MainMenuController* controller);
    void setReticleMenuController(ReticleMenuController* controller);
    void setColorMenuController(ColorMenuController* controller);
    void setZeroingController(ZeroingController* controller);
    void setWindageController(WindageController* controller);
    void setEnvironmentalController(EnvironmentalController* controller);
    void setZoneDefinitionController(ZoneDefinitionController* controller);
    void setSystemStatusController(SystemStatusController* controller);
    void setAboutController(AboutController* controller);
    void setSystemStateModel(SystemStateModel* model);

    // Initialization
    void initialize();

    // Menu state enum
    enum class MenuState {
        None,
        MainMenu,
        ReticleMenu,
        ColorMenu,
        ZeroingProcedure,
        WindageProcedure,
        EnvironmentalProcedure,
        ZoneDefinition,
        SystemStatus,
        RadarTargets,
        HelpAbout
    };

public slots:
    // Button handlers (called from QML or hardware controller)
    void onMenuValButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();
    void showMainMenu();

private slots:
    // Main menu action handlers
    void handlePersonalizeReticle();
    void handlePersonalizeColors();
    void handleZeroing();
    void handleClearZero();
    void handleWindage();
    void handleClearWindage();
    void handleEnvironmental();
    void handleClearEnvironmental();
    void handleZoneDefinitions();
    void handleSystemStatus();
    void handleToggleDetection();
    void handleShutdown();
    void handleRadarTargetList();
    void handleHelpAbout();

    // Completion handlers
    void handleMainMenuFinished();
    void handleReticleMenuFinished();
    void handleColorMenuFinished();
    void handleZeroingFinished();
    void handleWindageFinished();
    void handleEnvironmentalFinished();
    void handleZoneDefinitionFinished();
    void handleSystemStatusFinished();
    void handleAboutFinished();

    void handleReturnToMainMenu();

private slots:
    // Monitor button state changes from hardware
    void onSystemStateChanged(std::shared_ptr<const SystemStateData> newState);

private:
    void setMenuState(MenuState state);
    void hideAllMenus();

    // State-specific button handlers
    void handleMenuValInNoMenuState();
    void handleMenuValInMainMenu();
    void handleMenuValInSubmenu();
    void handleMenuValInProcedure();

    MenuState m_currentMenuState;

    // Button state tracking for edge detection (rising edge = button press)
    bool m_previousMenuUpState = false;
    bool m_previousMenuDownState = false;
    bool m_previousMenuValState = false;

    // Dependencies (injected by SystemController)
    MainMenuController* m_mainMenuController;
    ReticleMenuController* m_reticleMenuController;
    ColorMenuController* m_colorMenuController;
    ZeroingController* m_zeroingController;
    WindageController* m_windageController;
    EnvironmentalController* m_environmentalController;
    ZoneDefinitionController* m_zoneDefinitionController;
    SystemStatusController* m_systemStatusController;
    AboutController* m_aboutController;
    SystemStateModel* m_systemStateModel;
};

#endif // APPLICATIONCONTROLLER_H

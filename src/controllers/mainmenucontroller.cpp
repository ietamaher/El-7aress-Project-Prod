#include "controllers/mainmenucontroller.h"
#include "models/menuviewmodel.h"
#include "models/osdviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

MainMenuController::MainMenuController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
{
}

void MainMenuController::initialize()
{
    //m_viewModel = ServiceManager::instance()->get<MenuViewModel>(QString("MainMenuViewModel"));
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    connect(m_viewModel, &MenuViewModel::optionSelected,
            this, &MainMenuController::handleMenuOptionSelected);
    //SystemStateModel* stateModel = ServiceManager::instance()->get<SystemStateModel>();
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::colorStyleChanged,
                this, &MainMenuController::onColorStyleChanged);

        // ✅ LATENCY FIX: Use dedicated signals to reduce event queue load
        // Only processes events when camera/detection actually changes (2-5 times per session)
        // instead of 1,200 events/min from dataChanged signal
        connect(m_stateModel, &SystemStateModel::activeCameraChanged,
                this, &MainMenuController::onCameraChanged,
                Qt::QueuedConnection);  // Non-blocking signal delivery

        connect(m_stateModel, &SystemStateModel::detectionStateChanged,
                this, &MainMenuController::onDetectionChanged,
                Qt::QueuedConnection);  // Non-blocking signal delivery

        // Set initial color
        const auto& data = m_stateModel->data();
        m_viewModel->setAccentColor(data.colorStyle);
    }
}

QStringList MainMenuController::buildMainMenuOptions() const
{
    const auto& data = m_stateModel->data();

    // Build detection option with dynamic state display
    QString detectionOption;
    if (!data.activeCameraIsDay) {
        detectionOption = "Detection (Night - Unavailable)";
    } else {
        detectionOption = data.detectionEnabled
            ? "Detection: ENABLED"
            : "Detection: DISABLED";
    }

    // Build home calibration option with status
    QString homeCalibOption = data.azHomeOffsetApplied
        ? "Az Home Calibration [H]"
        : "Az Home Calibration";

    QStringList options;
    options << "--- RETICLE & DISPLAY ---"
            << "Personalize Reticle"
            << "Personalize Colors"
            << "--- BALLISTICS ---"
            << "Zeroing"
            << "Clear Active Zero"
            << "Windage"
            << "Clear Active Windage"
            << "Environmental Settings"
            << "Clear Environmental Settings"
            << "--- CALIBRATION ---"
            << homeCalibOption
            << "Clear Az Home Calibration"
            << "Preset Home Position"
            << "--- SYSTEM ---"
            << "Zone Definitions"
            // << "System Status"  // DISABLED
            << detectionOption
            << "Shutdown System"
            << "--- INFO ---"
            << "Help/About"
            << "Return ...";

    // NO "Return ..." option since pressing MENU/VAL again will close the menu

    return options;
}

void MainMenuController::show()
{
    // Save initial state for change detection
    const auto& data = m_stateModel->data();
    m_previousCameraIsDay = data.activeCameraIsDay;
    m_previousDetectionEnabled = data.detectionEnabled;

    QStringList menuOptions = buildMainMenuOptions();
    m_viewModel->showMenu("Main Menu", "Navigate with UP/DOWN, Select with MENU/VAL", menuOptions);
}

void MainMenuController::hide()
{
    m_viewModel->hideMenu();
}

void MainMenuController::onUpButtonPressed()
{
    m_viewModel->moveSelectionUp();
}

void MainMenuController::onDownButtonPressed()
{
    m_viewModel->moveSelectionDown();
}

void MainMenuController::onSelectButtonPressed()
{
    // This is called when MENU/VAL is pressed while in the main menu
    m_viewModel->selectCurrentItem();
}

void MainMenuController::handleMenuOptionSelected(const QString& option)
{
    qDebug() << "MainMenuController: Option selected:" << option;

    hide(); // Always hide the menu after selection

    // Route to appropriate handler based on selection
    if (option == "Personalize Reticle") {
        emit personalizeReticleRequested();
        emit menuFinished(); // ✅ Emit after each action
    }
    else if (option == "Personalize Colors") {
        emit personalizeColorsRequested();
        emit menuFinished();
    }
    else if (option == "Zeroing") {
        emit zeroingRequested();
        // ❌ DON'T emit menuFinished() - zeroing is a procedure
    }
    else if (option == "Clear Active Zero") {
        emit clearZeroRequested();
        emit menuFinished();
    }
    else if (option == "Windage") {
        emit windageRequested();
        // ❌ DON'T emit menuFinished() - windage is a procedure
    }
    else if (option == "Clear Active Windage") {
        emit clearWindageRequested();
        emit menuFinished();
    }
    else if (option == "Environmental Settings") {
        emit environmentalRequested();
        // ❌ DON'T emit menuFinished() - environmental is a procedure
    }
    else if (option == "Clear Environmental Settings") {
        emit clearEnvironmentalRequested();
        emit menuFinished();
    }
    else if (option.startsWith("Az Home Calibration")) {
        emit homeCalibrationRequested();
        // DON'T emit menuFinished() - home calibration is a procedure
    }
    else if (option == "Clear Az Home Calibration") {
        emit clearHomeCalibrationRequested();
        emit menuFinished();
    }
    else if (option == "Preset Home Position") {
        emit presetHomePositionRequested();
        // DON'T emit menuFinished() - preset home position is a procedure
    }
    else if (option == "Zone Definitions") {
        emit zoneDefinitionsRequested();
        // ❌ DON'T emit menuFinished() - zone definition is a procedure
    }
    // else if (option == "System Status") {  // DISABLED
    //     emit systemStatusRequested();  // DISABLED
    //    // emit menuFinished();  // DISABLED
    // }  // DISABLED
    else if (option.startsWith("Detection")) {
        if (option.contains("Unavailable")) {
            qDebug() << "Detection unavailable - Night camera is active";
            emit menuFinished();
        } else {
            emit toggleDetectionRequested();
            emit menuFinished();
        }
    }
    else if (option == "Shutdown System") {
        emit shutdownSystemRequested();
        emit menuFinished();
    }
    else if (option == "Radar Target List") {
        emit radarTargetListRequested();
        //emit menuFinished();
    }
    else if (option == "Help/About") {
        emit helpAboutRequested();
        // menuFinished();
    }
    else if (option == "Return ...") {
        qDebug() << "MainMenuController: Return option selected - closing menu";
        emit menuFinished();
    }
    else {
        qWarning() << "MainMenuController: Unknown option:" << option;
    }
}

void MainMenuController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "MainMenuController: Color changed to" << color;
    m_viewModel->setAccentColor(color);
}

void MainMenuController::onCameraChanged(bool isDayCamera)
{
    // Only update if menu is visible
    if (!m_viewModel->visible()) return;

    qDebug() << "MainMenuController: Camera changed while menu open - rebuilding options";
    m_previousCameraIsDay = isDayCamera;

    // Save current selection
    int currentIndex = m_viewModel->currentIndex();

    // Rebuild menu with updated options
    QStringList menuOptions = buildMainMenuOptions();
    m_viewModel->optionsModel()->setStringList(menuOptions);

    // Restore selection if still valid
    if (currentIndex >= 0 && currentIndex < menuOptions.count()) {
        m_viewModel->setCurrentIndex(currentIndex);
    }
}

void MainMenuController::onDetectionChanged(bool enabled)
{
    // Only update if menu is visible
    if (!m_viewModel->visible()) return;

    m_previousDetectionEnabled = enabled;

    // Save current selection
    int currentIndex = m_viewModel->currentIndex();

    // Rebuild menu with updated detection option
    QStringList menuOptions = buildMainMenuOptions();
    m_viewModel->optionsModel()->setStringList(menuOptions);

    // Restore selection
    if (currentIndex >= 0 && currentIndex < menuOptions.count()) {
        m_viewModel->setCurrentIndex(currentIndex);
    }
}

void MainMenuController::setViewModel(MenuViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void MainMenuController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}


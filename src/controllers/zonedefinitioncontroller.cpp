#include "zonedefinitioncontroller.h"
#include "models/zonedefinitionviewmodel.h"
#include "models/zonemapviewmodel.h"
#include "models/areazoneparameterviewmodel.h"
#include "models/sectorscanparameterviewmodel.h"
#include "models/trpparameterviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QCoreApplication>
#include <algorithm>

ZoneDefinitionController::ZoneDefinitionController(QObject* parent)
    : QObject(parent), m_currentState(State::Idle_MainMenu), m_viewModel(nullptr),
      m_mapViewModel(nullptr), m_areaZoneParamViewModel(nullptr),
      m_sectorScanParamViewModel(nullptr), m_trpParamViewModel(nullptr), m_stateModel(nullptr),
      m_editingZoneId(-1), m_wipZoneType(ZoneType::None), m_deleteZoneType(ZoneType::None),
      m_corner1Defined(false), m_wipAz1(0.0f), m_wipEl1(0.0f), m_wipAz2(0.0f), m_wipEl2(0.0f),
      m_currentGimbalAz(0.0f), m_currentGimbalEl(0.0f), m_currentMenuIndex(0) {}

void ZoneDefinitionController::initialize() {
    // Get ViewModels from ServiceManager
    /*m_viewModel = ServiceManager::instance()->get<ZoneDefinitionViewModel>();
    m_mapViewModel = ServiceManager::instance()->get<ZoneMapViewModel>();
    m_areaZoneParamViewModel = ServiceManager::instance()->get<AreaZoneParameterViewModel>();
    m_sectorScanParamViewModel = ServiceManager::instance()->get<SectorScanParameterViewModel>();
    m_trpParamViewModel = ServiceManager::instance()->get<TRPParameterViewModel>();
*/
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_mapViewModel);
    Q_ASSERT(m_areaZoneParamViewModel);
    Q_ASSERT(m_sectorScanParamViewModel);
    Q_ASSERT(m_trpParamViewModel);

    // Get SystemStateModel
    //m_stateModel = ServiceManager::instance()->get<SystemStateModel>();
    Q_ASSERT(m_stateModel);

    // Connect to model signals
    connect(m_stateModel, &SystemStateModel::gimbalPositionChanged, this,
            &ZoneDefinitionController::onGimbalPositionChanged);
    connect(m_stateModel, &SystemStateModel::zonesChanged, this,
            &ZoneDefinitionController::onZonesChanged);
    connect(m_stateModel, &SystemStateModel::colorStyleChanged, this,
            &ZoneDefinitionController::onColorStyleChanged);
    connect(m_stateModel, &SystemStateModel::colorStyleChanged, this,
            &ZoneDefinitionController::onColorStyleChanged);
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
    qDebug() << "ZoneDefinitionController initialized";
}

void ZoneDefinitionController::show() {
    qDebug() << "ZoneDefinitionController: show() called";
    m_currentState = State::Idle_MainMenu;
    resetWipData();
    m_viewModel->setVisible(true);

    // Update initial gimbal position
    const auto& data = m_stateModel->data();
    m_currentGimbalAz = data.gimbalAz;
    m_currentGimbalEl = data.gimbalEl;
    m_viewModel->setGimbalPosition(m_currentGimbalAz, m_currentGimbalEl);
    m_mapViewModel->setGimbalPosition(m_currentGimbalAz, m_currentGimbalEl);

    // Load initial zones
    m_mapViewModel->updateZones(m_stateModel);

    updateUI();
}

void ZoneDefinitionController::hide() {
    qDebug() << "ZoneDefinitionController: hide() called";
    m_viewModel->setVisible(false);
    m_mapViewModel->clearWipZone();
}

void ZoneDefinitionController::resetWipData() {
    m_wipZoneType = ZoneType::None;
    m_wipAreaZone = AreaZone{};
    m_wipSectorScan = AutoSectorScanZone{};
    m_wipTRP = TargetReferencePoint{};
    m_editingZoneId = -1;
    m_deleteZoneType = ZoneType::None;
    m_corner1Defined = false;
    m_wipAz1 = m_wipEl1 = m_wipAz2 = m_wipEl2 = 0.0f;
    m_currentMenuIndex = 0;
    m_mapViewModel->clearWipZone();
    m_mapViewModel->setHighlightedZone(-1);
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void ZoneDefinitionController::onUpButtonPressed() {
    switch (m_currentState) {
    case State::Idle_MainMenu:
    case State::Select_ZoneType_ForNew:
    case State::Select_ZoneType_ForModify:
    case State::Select_ZoneType_ForDelete:
    case State::Select_AreaZone_ToModify:
    case State::Select_SectorScan_ToModify:
    case State::Select_TRP_ToModify:
    case State::Select_AreaZone_ToDelete:
    case State::Select_SectorScan_ToDelete:
    case State::Select_TRP_ToDelete:
    case State::Confirm_Save:
    case State::Confirm_Delete:
        // Menu navigation
        if (m_currentMenuIndex > 0) {
            m_currentMenuIndex--;
            m_viewModel->setCurrentIndex(m_currentMenuIndex);
        }
        break;

    case State::AreaZone_Edit_Parameters:
    case State::SectorScan_Edit_Parameters:
    case State::TRP_Edit_Parameters:
        routeUpToParameterPanel();
        break;

    default:
        break;
    }
}

void ZoneDefinitionController::onDownButtonPressed() {
    switch (m_currentState) {
    case State::Idle_MainMenu:
    case State::Select_ZoneType_ForNew:
    case State::Select_ZoneType_ForModify:
    case State::Select_ZoneType_ForDelete:
    case State::Select_AreaZone_ToModify:
    case State::Select_SectorScan_ToModify:
    case State::Select_TRP_ToModify:
    case State::Select_AreaZone_ToDelete:
    case State::Select_SectorScan_ToDelete:
    case State::Select_TRP_ToDelete:
    case State::Confirm_Save:
    case State::Confirm_Delete:
        // Menu navigation
        if (m_currentMenuIndex < m_currentMenuItems.size() - 1) {
            m_currentMenuIndex++;
            m_viewModel->setCurrentIndex(m_currentMenuIndex);
        }
        break;

    case State::AreaZone_Edit_Parameters:
    case State::SectorScan_Edit_Parameters:
    case State::TRP_Edit_Parameters:
        routeDownToParameterPanel();
        break;

    default:
        break;
    }
}

void ZoneDefinitionController::onMenuValButtonPressed() {
    qDebug() << "ZoneDefinitionController: MENU/VAL pressed in state"
             << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case State::Idle_MainMenu:
        processMainMenuSelect();
        break;

    case State::Select_ZoneType_ForNew:
        processSelectZoneTypeSelect();
        break;

    case State::Select_ZoneType_ForModify:
    case State::Select_ZoneType_ForDelete:
        processSelectZoneTypeForModifyDeleteSelect();
        break;

    case State::Select_AreaZone_ToModify:
    case State::Select_SectorScan_ToModify:
    case State::Select_TRP_ToModify:
    case State::Select_AreaZone_ToDelete:
    case State::Select_SectorScan_ToDelete:
    case State::Select_TRP_ToDelete:
        processSelectExistingZoneSelect();
        break;

    case State::AreaZone_Aim_Corner1:
    case State::AreaZone_Aim_Corner2:
    case State::SectorScan_Aim_Point1:
    case State::SectorScan_Aim_Point2:
    case State::TRP_Aim_Point:
        processAimPointConfirm();
        break;

    case State::AreaZone_Edit_Parameters:
    case State::SectorScan_Edit_Parameters:
    case State::TRP_Edit_Parameters:
        routeSelectToParameterPanel();
        break;

    case State::Confirm_Save:
        processConfirmSaveSelect();
        break;

    case State::Confirm_Delete:
        processConfirmDeleteSelect();
        break;

    case State::Show_Message:
        transitionToState(State::Idle_MainMenu);
        break;

    default:
        qWarning() << "Unhandled state in onMenuValButtonPressed:"
                   << static_cast<int>(m_currentState);
        break;
    }
}

// ============================================================================
// MODEL SIGNAL HANDLERS
// ============================================================================

void ZoneDefinitionController::onGimbalPositionChanged(float az, float el) {
    m_currentGimbalAz = az;
    m_currentGimbalEl = el;
    m_viewModel->setGimbalPosition(az, el);
    m_mapViewModel->setGimbalPosition(az, el);

    // Update WIP zone if in aiming state
    switch (m_currentState) {
    case State::AreaZone_Aim_Corner1:
    case State::AreaZone_Aim_Corner2:
    case State::SectorScan_Aim_Point1:
    case State::SectorScan_Aim_Point2:
    case State::TRP_Aim_Point:
        updateMapWipZone();
        break;
    default:
        break;
    }
}

void ZoneDefinitionController::onZonesChanged() {
    qDebug() << "ZoneDefinitionController: Received zonesChanged signal";

    // Refresh zone data in map
    m_mapViewModel->updateZones(m_stateModel);

    // If in a zone selection state, refresh the list
    switch (m_currentState) {
    case State::Select_AreaZone_ToModify:
        setupSelectExistingZoneUI(ZoneType::Safety, "Modify Area Zone");
        break;
    case State::Select_SectorScan_ToModify:
        setupSelectExistingZoneUI(ZoneType::AutoSectorScan, "Modify Sector Scan Zone");
        break;
    case State::Select_TRP_ToModify:
        setupSelectExistingZoneUI(ZoneType::TargetReferencePoint, "Modify TRP");
        break;
    case State::Select_AreaZone_ToDelete:
        setupSelectExistingZoneUI(ZoneType::Safety, "Delete Area Zone");
        break;
    case State::Select_SectorScan_ToDelete:
        setupSelectExistingZoneUI(ZoneType::AutoSectorScan, "Delete Sector Scan Zone");
        break;
    case State::Select_TRP_ToDelete:
        setupSelectExistingZoneUI(ZoneType::TargetReferencePoint, "Delete TRP");
        break;
    default:
        break;
    }
}

void ZoneDefinitionController::onColorStyleChanged(const QColor& color) {
    qDebug() << "ZoneDefinitionController: Color changed to" << color;
    m_viewModel->setAccentColor(color);
}

// ============================================================================
// STATE TRANSITIONS & UI UPDATES
// ============================================================================

void ZoneDefinitionController::transitionToState(State newState) {
    qDebug() << "ZoneDefinitionController: Transitioning from" << static_cast<int>(m_currentState)
             << "to" << static_cast<int>(newState);
    m_currentState = newState;
    updateUI();
}

void ZoneDefinitionController::updateUI() {
    switch (m_currentState) {
    case State::Idle_MainMenu:
        setupIdleMainMenuUI();
        break;
    case State::Select_ZoneType_ForNew:
        setupSelectZoneTypeUI();
        break;
    case State::Select_ZoneType_ForModify:
        setupSelectZoneTypeForModifyDeleteUI("Modify");
        break;
    case State::Select_ZoneType_ForDelete:
        setupSelectZoneTypeForModifyDeleteUI("Delete");
        break;
    case State::Select_AreaZone_ToModify:
        setupSelectExistingZoneUI(ZoneType::Safety, "Modify Area Zone");
        break;
    case State::Select_SectorScan_ToModify:
        setupSelectExistingZoneUI(ZoneType::AutoSectorScan, "Modify Sector Scan Zone");
        break;
    case State::Select_TRP_ToModify:
        setupSelectExistingZoneUI(ZoneType::TargetReferencePoint, "Modify TRP");
        break;
    case State::Select_AreaZone_ToDelete:
        setupSelectExistingZoneUI(ZoneType::Safety, "Delete Area Zone");
        break;
    case State::Select_SectorScan_ToDelete:
        setupSelectExistingZoneUI(ZoneType::AutoSectorScan, "Delete Sector Scan Zone");
        break;
    case State::Select_TRP_ToDelete:
        setupSelectExistingZoneUI(ZoneType::TargetReferencePoint, "Delete TRP");
        break;
    case State::AreaZone_Aim_Corner1:
        setupAimPointUI("Aim at FIRST corner (Az/El) and press MENU/VAL.");
        break;
    case State::AreaZone_Aim_Corner2:
        setupAimPointUI("Aim at SECOND corner (Az/El) and press MENU/VAL.");
        break;
    case State::AreaZone_Edit_Parameters:
        setupAreaZoneParametersUI(m_editingZoneId == -1);
        break;
    case State::SectorScan_Aim_Point1:
        setupAimPointUI("Aim at Sector Scan START point (Az/El) and press MENU/VAL.");
        break;
    case State::SectorScan_Aim_Point2:
        setupAimPointUI("Aim at Sector Scan END point (Az/El) and press MENU/VAL.");
        break;
    case State::SectorScan_Edit_Parameters:
        setupSectorScanParametersUI(m_editingZoneId == -1);
        break;
    case State::TRP_Aim_Point:
        setupAimPointUI("Aim at Target Reference Point (Az/El) and press MENU/VAL.");
        break;
    case State::TRP_Edit_Parameters:
        setupTRPParametersUI(m_editingZoneId == -1);
        break;
    case State::Confirm_Save:
        setupConfirmUI("Confirm Save", "Save Zone Definition?");
        break;
    case State::Confirm_Delete:
        setupConfirmUI("Confirm Delete", QString("Delete Zone ID %1?").arg(m_editingZoneId));
        break;
    case State::Show_Message:
        // Message already set by setupShowMessageUI
        break;
    }

    // Update map WIP zone for aiming states
    updateMapWipZone();
}

// ============================================================================
// UI SETUP METHODS
// ============================================================================

void ZoneDefinitionController::setupIdleMainMenuUI() {
    m_viewModel->setTitle("Zone Definition Menu");
    m_viewModel->setInstruction("Select option using UP/DOWN, confirm with MENU/VAL.");

    m_currentMenuItems = {"New Zone", "Modify Zone", "Delete Zone", "Return"};
    m_viewModel->setMenuOptions(m_currentMenuItems);
    m_currentMenuIndex = 0;
    m_viewModel->setCurrentIndex(m_currentMenuIndex);

    m_viewModel->setShowMainMenu(true);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(false);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
}

void ZoneDefinitionController::setupSelectZoneTypeUI() {
    m_viewModel->setTitle("Select Zone Type");
    m_viewModel->setInstruction("Choose type to create.");

    m_currentMenuItems = {"Safety Zone", "No-Traverse Zone", "No-Fire Zone",
                          "Sector Scan", "Target Ref Point", "Back"};
    m_viewModel->setMenuOptions(m_currentMenuItems);
    m_currentMenuIndex = 0;
    m_viewModel->setCurrentIndex(m_currentMenuIndex);

    m_viewModel->setShowMainMenu(true);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(false);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
}

void ZoneDefinitionController::setupSelectZoneTypeForModifyDeleteUI(const QString& action) {
    m_viewModel->setTitle(QString("%1 Zone - Select Type").arg(action));
    m_viewModel->setInstruction("Select zone type using UP/DOWN, confirm with MENU/VAL.");

    m_currentMenuItems = {"Area Zone", "Sector Scan", "TRP", "Back"};
    m_viewModel->setMenuOptions(m_currentMenuItems);
    m_currentMenuIndex = 0;
    m_viewModel->setCurrentIndex(m_currentMenuIndex);

    m_viewModel->setShowMainMenu(true);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(false);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
}

void ZoneDefinitionController::setupSelectExistingZoneUI(ZoneType typeToSelect,
                                                         const QString& title) {
    m_viewModel->setTitle(title);
    m_viewModel->setInstruction("Select zone using UP/DOWN, confirm with MENU/VAL.");

    m_currentMenuItems.clear();
    QStringList displayItems;

    // Populate list based on typeToSelect
    if (typeToSelect == ZoneType::Safety || typeToSelect == ZoneType::NoFire ||
        typeToSelect == ZoneType::NoTraverse) {
        const auto& zones = m_stateModel->getAreaZones();
        qDebug() << "setupSelectExistingZoneUI: Found" << zones.size() << "area zones";

        for (const auto& zone : zones) {
            QString zoneTypeName;
            switch (zone.type) {
            case ZoneType::Safety:
                zoneTypeName = "Safety";
                break;
            case ZoneType::NoTraverse:
                zoneTypeName = "No-Traverse";
                break;
            case ZoneType::NoFire:
                zoneTypeName = "No-Fire";
                break;
            default:
                zoneTypeName = "Unknown";
                break;
            }

            QString itemText = QString("ID: %1 (%2) %3")
                                   .arg(zone.id)
                                   .arg(zoneTypeName)
                                   .arg(zone.isEnabled ? "Enabled" : "Disabled");
            displayItems.append(itemText);
            m_currentMenuItems.append(QString::number(zone.id));
        }
    } else if (typeToSelect == ZoneType::AutoSectorScan) {
        const auto& zones = m_stateModel->getSectorScanZones();
        qDebug() << "setupSelectExistingZoneUI: Found" << zones.size() << "sector scan zones";

        for (const auto& zone : zones) {
            QString itemText = QString("ID: %1 (Sector Scan) %2")
                                   .arg(zone.id)
                                   .arg(zone.isEnabled ? "Enabled" : "Disabled");
            displayItems.append(itemText);
            m_currentMenuItems.append(QString::number(zone.id));
        }
    } else if (typeToSelect == ZoneType::TargetReferencePoint) {
        const auto& zones = m_stateModel->getTargetReferencePoints();
        qDebug() << "setupSelectExistingZoneUI: Found" << zones.size() << "TRP zones";

        for (const auto& zone : zones) {
            QString itemText = QString("ID: %1 (TRP) Page:%2 Idx:%3")
                                   .arg(zone.id)
                                   .arg(zone.locationPage)
                                   .arg(zone.trpInPage);
            displayItems.append(itemText);
            m_currentMenuItems.append(QString::number(zone.id));
        }
    }

    // Always add "Back" option
    displayItems.append("Back");
    m_currentMenuItems.append("Back");

    // If no zones found, add informative message
    if (m_currentMenuItems.size() == 1) {  // Only "Back"
        QString noZoneMessage;
        if (typeToSelect == ZoneType::AutoSectorScan) {
            noZoneMessage = "No Sector Scan zones defined";
        } else if (typeToSelect == ZoneType::TargetReferencePoint) {
            noZoneMessage = "No TRP zones defined";
        } else {
            noZoneMessage = "No Area zones defined";
        }
        displayItems.insert(0, noZoneMessage);
        m_currentMenuItems.insert(0, "NoZones");
    }

    m_viewModel->setMenuOptions(displayItems);
    m_currentMenuIndex = 0;
    m_viewModel->setCurrentIndex(m_currentMenuIndex);

    m_viewModel->setShowMainMenu(false);
    m_viewModel->setShowZoneSelectionList(true);
    m_viewModel->setShowParameterPanel(false);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
}

void ZoneDefinitionController::setupAimPointUI(const QString& instructionText) {
    m_viewModel->setTitle("Define Zone Geometry");
    m_viewModel->setInstruction(instructionText);

    m_viewModel->setShowMainMenu(false);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(false);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
}

void ZoneDefinitionController::setupAreaZoneParametersUI(bool isNew) {
    qDebug() << "Setting up AreaZone parameters UI";

    QString title = isNew ? "Set New Area Zone Parameters" : "Modify Area Zone Parameters";
    m_viewModel->setTitle(title);
    m_viewModel->setInstruction(
        "Configure area zone parameters using UP/DOWN to navigate, MENU/VAL to toggle/confirm");

    // Set up parameter ViewModel with current WIP data
    m_areaZoneParamViewModel->setIsEnabled(m_wipAreaZone.isEnabled);
    m_areaZoneParamViewModel->setIsOverridable(m_wipAreaZone.isOverridable);
    m_areaZoneParamViewModel->setActiveField(AreaZoneParameterViewModel::Field::Enabled);

    m_viewModel->setShowMainMenu(false);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(true);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
    m_viewModel->setActivePanelType(ZoneDefinitionViewModel::PanelType::AreaZone);

    updateMapWipZone();
}

void ZoneDefinitionController::setupSectorScanParametersUI(bool isNew) {
    QString title =
        isNew ? "New Sector Scan Zone - Parameters" : "Modify Sector Scan Zone - Parameters";
    m_viewModel->setTitle(title);
    m_viewModel->setInstruction("Configure parameters using UP/DOWN, MENU/VAL to edit values.");

    // Set up parameter ViewModel
    m_sectorScanParamViewModel->setIsEnabled(m_wipSectorScan.isEnabled);
    m_sectorScanParamViewModel->setScanSpeed(static_cast<int>(m_wipSectorScan.scanSpeed));
    m_sectorScanParamViewModel->setActiveField(SectorScanParameterViewModel::Field::Enabled);
    m_sectorScanParamViewModel->setIsEditingValue(false);

    m_viewModel->setShowMainMenu(false);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(true);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
    m_viewModel->setActivePanelType(ZoneDefinitionViewModel::PanelType::SectorScan);

    updateMapWipZone();
}

void ZoneDefinitionController::setupTRPParametersUI(bool isNew) {
    QString title = isNew ? "New TRP - Parameters" : "Modify TRP - Parameters";
    m_viewModel->setTitle(title);
    m_viewModel->setInstruction("Configure parameters using UP/DOWN, MENU/VAL to edit values.");

    // Set up parameter ViewModel
    m_trpParamViewModel->setLocationPage(m_wipTRP.locationPage);
    m_trpParamViewModel->setTrpInPage(m_wipTRP.trpInPage);
    m_trpParamViewModel->setHaltTime(m_wipTRP.haltTime);
    m_trpParamViewModel->setActiveField(TRPParameterViewModel::Field::LocationPage);
    m_trpParamViewModel->setIsEditingValue(false);

    m_viewModel->setShowMainMenu(false);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(true);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
    m_viewModel->setActivePanelType(ZoneDefinitionViewModel::PanelType::TRP);

    updateMapWipZone();
}

void ZoneDefinitionController::setupConfirmUI(const QString& title, const QString& question) {
    m_viewModel->setTitle(title);
    m_viewModel->setInstruction(question);

    m_currentMenuItems = {"Yes", "No"};
    m_viewModel->setMenuOptions(m_currentMenuItems);
    m_currentMenuIndex = 0;  // Default to Yes
    m_viewModel->setCurrentIndex(m_currentMenuIndex);

    m_viewModel->setShowMainMenu(false);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(false);
    m_viewModel->setShowConfirmDialog(true);
    m_viewModel->setShowMap(true);
}

void ZoneDefinitionController::setupShowMessageUI(const QString& message) {
    m_viewModel->setTitle("Information");
    m_viewModel->setInstruction(message + "\nPress MENU/VAL to return.");

    m_viewModel->setShowMainMenu(false);
    m_viewModel->setShowZoneSelectionList(false);
    m_viewModel->setShowParameterPanel(false);
    m_viewModel->setShowConfirmDialog(false);
    m_viewModel->setShowMap(true);
}

// ============================================================================
// ACTION PROCESSORS
// ============================================================================

void ZoneDefinitionController::processMainMenuSelect() {
    QString selectedOption = m_currentMenuItems.value(m_currentMenuIndex);
    qDebug() << "processMainMenuSelect:" << selectedOption;

    if (selectedOption == "New Zone") {
        transitionToState(State::Select_ZoneType_ForNew);
    } else if (selectedOption == "Modify Zone") {
        transitionToState(State::Select_ZoneType_ForModify);
    } else if (selectedOption == "Delete Zone") {
        transitionToState(State::Select_ZoneType_ForDelete);
    } else if (selectedOption == "Return") {
        hide();
        emit returnToMainMenu();
    }
}

void ZoneDefinitionController::processSelectZoneTypeSelect() {
    QString selectedType = m_currentMenuItems.value(m_currentMenuIndex);
    qDebug() << "processSelectZoneTypeSelect:" << selectedType;

    resetWipData();
    m_editingZoneId = -1;

    if (selectedType == "Safety Zone") {
        m_wipZoneType = ZoneType::Safety;
        m_wipAreaZone.type = ZoneType::Safety;
        transitionToState(State::AreaZone_Aim_Corner1);
    } else if (selectedType == "No-Traverse Zone") {
        m_wipZoneType = ZoneType::NoTraverse;
        m_wipAreaZone.type = ZoneType::NoTraverse;
        transitionToState(State::AreaZone_Aim_Corner1);
    } else if (selectedType == "No-Fire Zone") {
        m_wipZoneType = ZoneType::NoFire;
        m_wipAreaZone.type = ZoneType::NoFire;
        transitionToState(State::AreaZone_Aim_Corner1);
    } else if (selectedType == "Sector Scan") {
        m_wipZoneType = ZoneType::AutoSectorScan;
        transitionToState(State::SectorScan_Aim_Point1);
    } else if (selectedType == "Target Ref Point") {
        m_wipZoneType = ZoneType::TargetReferencePoint;
        transitionToState(State::TRP_Aim_Point);
    } else if (selectedType == "Back") {
        transitionToState(State::Idle_MainMenu);
    }
}

void ZoneDefinitionController::processSelectZoneTypeForModifyDeleteSelect() {
    if (m_currentMenuIndex >= 0 && m_currentMenuIndex < m_currentMenuItems.size()) {
        QString selectedType = m_currentMenuItems[m_currentMenuIndex];
        qDebug() << "processSelectZoneTypeForModifyDeleteSelect:" << selectedType;

        if (selectedType == "Back") {
            transitionToState(State::Idle_MainMenu);
        } else {
            if (m_currentState == State::Select_ZoneType_ForModify) {
                if (selectedType == "Area Zone") {
                    transitionToState(State::Select_AreaZone_ToModify);
                } else if (selectedType == "Sector Scan") {
                    transitionToState(State::Select_SectorScan_ToModify);
                } else if (selectedType == "TRP") {
                    transitionToState(State::Select_TRP_ToModify);
                }
            } else if (m_currentState == State::Select_ZoneType_ForDelete) {
                if (selectedType == "Area Zone") {
                    transitionToState(State::Select_AreaZone_ToDelete);
                } else if (selectedType == "Sector Scan") {
                    transitionToState(State::Select_SectorScan_ToDelete);
                } else if (selectedType == "TRP") {
                    transitionToState(State::Select_TRP_ToDelete);
                }
            }
        }
    }
}

void ZoneDefinitionController::processSelectExistingZoneSelect() {
    if (m_currentMenuIndex >= 0 && m_currentMenuIndex < m_currentMenuItems.size()) {
        QString selectedItem = m_currentMenuItems[m_currentMenuIndex];
        qDebug() << "processSelectExistingZoneSelect:" << selectedItem;

        if (selectedItem == "Back") {
            // Return to zone type selection
            switch (m_currentState) {
            case State::Select_AreaZone_ToModify:
            case State::Select_SectorScan_ToModify:
            case State::Select_TRP_ToModify:
                transitionToState(State::Select_ZoneType_ForModify);
                break;
            case State::Select_AreaZone_ToDelete:
            case State::Select_SectorScan_ToDelete:
            case State::Select_TRP_ToDelete:
                transitionToState(State::Select_ZoneType_ForDelete);
                break;
            default:
                transitionToState(State::Idle_MainMenu);
                break;
            }
        } else if (selectedItem == "NoZones") {
            // Ignore selection of "No zones" message
            return;
        } else {
            // Zone ID selected
            m_editingZoneId = selectedItem.toInt();
            qDebug() << "Selected zone ID:" << m_editingZoneId;

            // Determine next state based on current state
            switch (m_currentState) {
            case State::Select_AreaZone_ToModify:
                if (auto* zone = m_stateModel->getAreaZoneById(m_editingZoneId)) {
                    m_wipAreaZone = *zone;
                    m_wipZoneType = zone->type;
                    transitionToState(State::AreaZone_Edit_Parameters);
                } else {
                    setupShowMessageUI("Zone not found!");
                    transitionToState(State::Show_Message);
                }
                break;

            case State::Select_SectorScan_ToModify:
                if (auto* zone = m_stateModel->getSectorScanZoneById(m_editingZoneId)) {
                    m_wipSectorScan = *zone;
                    m_wipZoneType = ZoneType::AutoSectorScan;
                    transitionToState(State::SectorScan_Edit_Parameters);
                } else {
                    setupShowMessageUI("Zone not found!");
                    transitionToState(State::Show_Message);
                }
                break;

            case State::Select_TRP_ToModify:
                if (auto* trp = m_stateModel->getTRPById(m_editingZoneId)) {
                    m_wipTRP = *trp;
                    m_wipZoneType = ZoneType::TargetReferencePoint;
                    transitionToState(State::TRP_Edit_Parameters);
                } else {
                    setupShowMessageUI("Zone not found!");
                    transitionToState(State::Show_Message);
                }
                break;

            case State::Select_AreaZone_ToDelete:
                m_deleteZoneType = ZoneType::Safety;
                transitionToState(State::Confirm_Delete);
                break;

            case State::Select_SectorScan_ToDelete:
                m_deleteZoneType = ZoneType::AutoSectorScan;
                transitionToState(State::Confirm_Delete);
                break;

            case State::Select_TRP_ToDelete:
                m_deleteZoneType = ZoneType::TargetReferencePoint;
                transitionToState(State::Confirm_Delete);
                break;

            default:
                qWarning() << "Unexpected state in processSelectExistingZoneSelect";
                break;
            }
        }
    }
}

void ZoneDefinitionController::processAimPointConfirm() {
    qDebug() << "processAimPointConfirm in state" << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case State::AreaZone_Aim_Corner1:
        m_wipAz1 = m_currentGimbalAz;
        m_wipEl1 = m_currentGimbalEl;
        m_corner1Defined = true;
        qDebug() << "AreaZone Corner 1 captured: Az=" << m_wipAz1 << "El=" << m_wipEl1;
        transitionToState(State::AreaZone_Aim_Corner2);
        break;

    case State::AreaZone_Aim_Corner2:
        if (!m_corner1Defined) {
            setupShowMessageUI("Error: Corner 1 not defined.");
            transitionToState(State::Show_Message);
            return;
        }
        m_wipAz2 = m_currentGimbalAz;
        m_wipEl2 = m_currentGimbalEl;
        qDebug() << "AreaZone Corner 2 captured: Az=" << m_wipAz2 << "El=" << m_wipEl2;
        calculateAreaZoneGeometry();
        transitionToState(State::AreaZone_Edit_Parameters);
        break;

    case State::SectorScan_Aim_Point1:
        m_wipSectorScan.az1 = m_currentGimbalAz;
        m_wipSectorScan.el1 = m_currentGimbalEl;
        qDebug() << "SectorScan Point 1 captured: Az=" << m_wipSectorScan.az1
                 << "El=" << m_wipSectorScan.el1;
        transitionToState(State::SectorScan_Aim_Point2);
        break;

    case State::SectorScan_Aim_Point2:
        m_wipSectorScan.az2 = m_currentGimbalAz;
        m_wipSectorScan.el2 = m_currentGimbalEl;
        qDebug() << "SectorScan Point 2 captured: Az=" << m_wipSectorScan.az2
                 << "El=" << m_wipSectorScan.el2;
        transitionToState(State::SectorScan_Edit_Parameters);
        break;

    case State::TRP_Aim_Point:
        m_wipTRP.azimuth = m_currentGimbalAz;
        m_wipTRP.elevation = m_currentGimbalEl;
        qDebug() << "TRP Point captured: Az=" << m_wipTRP.azimuth << "El=" << m_wipTRP.elevation;
        transitionToState(State::TRP_Edit_Parameters);
        break;

    default:
        qWarning() << "processAimPointConfirm called in unexpected state";
        break;
    }
}

void ZoneDefinitionController::processEditParametersConfirm() {
    qDebug() << "processEditParametersConfirm in state" << static_cast<int>(m_currentState);

    switch (m_currentState) {
    case State::AreaZone_Edit_Parameters:
        // Get data from ViewModel
        m_wipAreaZone.isEnabled = m_areaZoneParamViewModel->isEnabled();
        m_wipAreaZone.isOverridable = m_areaZoneParamViewModel->isOverridable();
        transitionToState(State::Confirm_Save);
        break;

    case State::SectorScan_Edit_Parameters:
        // Get data from ViewModel
        m_wipSectorScan.isEnabled = m_sectorScanParamViewModel->isEnabled();
        m_wipSectorScan.scanSpeed = static_cast<float>(m_sectorScanParamViewModel->scanSpeed());
        transitionToState(State::Confirm_Save);
        break;

    case State::TRP_Edit_Parameters:
        // Get data from ViewModel
        m_wipTRP.locationPage = m_trpParamViewModel->locationPage();
        m_wipTRP.trpInPage = m_trpParamViewModel->trpInPage();
        m_wipTRP.haltTime = m_trpParamViewModel->haltTime();
        transitionToState(State::Confirm_Save);
        break;

    default:
        qWarning() << "processEditParametersConfirm called in unexpected state";
        break;
    }
}

void ZoneDefinitionController::processConfirmSaveSelect() {
    QString selectedOption = m_currentMenuItems.value(m_currentMenuIndex);
    qDebug() << "processConfirmSaveSelect:" << selectedOption;

    if (selectedOption == "Yes") {
        bool success = false;
        QString errorMsg = "Error: Failed to save zone.";

        if (!m_stateModel) {
            errorMsg = "Error: SystemStateModel is not available.";
        } else {
            if (m_editingZoneId == -1) {
                // ADD new zone
                switch (m_wipZoneType) {
                case ZoneType::Safety:
                case ZoneType::NoTraverse:
                case ZoneType::NoFire:
                    success = m_stateModel->addAreaZone(m_wipAreaZone);
                    break;
                case ZoneType::AutoSectorScan:
                    success = m_stateModel->addSectorScanZone(m_wipSectorScan);
                    break;
                case ZoneType::TargetReferencePoint:
                    success = m_stateModel->addTRP(m_wipTRP);
                    break;
                default:
                    errorMsg = "Error: Unknown zone type to add.";
                    break;
                }
            } else {
                // MODIFY existing zone
                switch (m_wipZoneType) {
                case ZoneType::Safety:
                case ZoneType::NoTraverse:
                case ZoneType::NoFire:
                    success = m_stateModel->modifyAreaZone(m_editingZoneId, m_wipAreaZone);
                    break;
                case ZoneType::AutoSectorScan:
                    success = m_stateModel->modifySectorScanZone(m_editingZoneId, m_wipSectorScan);
                    break;
                case ZoneType::TargetReferencePoint:
                    success = m_stateModel->modifyTRP(m_editingZoneId, m_wipTRP);
                    break;
                default:
                    errorMsg = "Error: Unknown zone type to modify.";
                    break;
                }
            }
        }

        if (success) {
            // Save to file
            QString zonesPath = QCoreApplication::applicationDirPath() + "/config/zones.json";
            if (m_stateModel->saveZonesToFile(zonesPath)) {
                qDebug() << "Zones successfully saved to" << zonesPath;
            } else {
                qWarning() << "Failed to save zones to" << zonesPath;
            }

            resetWipData();
            transitionToState(State::Idle_MainMenu);
        } else {
            setupShowMessageUI(errorMsg);
            transitionToState(State::Show_Message);
        }
    } else {
        // No - return to main menu
        resetWipData();
        transitionToState(State::Idle_MainMenu);
    }
}

void ZoneDefinitionController::processConfirmDeleteSelect() {
    if (m_currentMenuIndex >= 0 && m_currentMenuIndex < m_currentMenuItems.size()) {
        QString selectedItem = m_currentMenuItems[m_currentMenuIndex];
        qDebug() << "processConfirmDeleteSelect:" << selectedItem;

        if (selectedItem == "Yes") {
            bool success = false;
            QString zoneTypeName;

            // Determine type and delete
            if (m_deleteZoneType == ZoneType::Safety || m_deleteZoneType == ZoneType::NoTraverse ||
                m_deleteZoneType == ZoneType::NoFire) {
                success = m_stateModel->deleteAreaZone(m_editingZoneId);
                zoneTypeName = "Area Zone";
            } else if (m_deleteZoneType == ZoneType::AutoSectorScan) {
                success = m_stateModel->deleteSectorScanZone(m_editingZoneId);
                zoneTypeName = "Sector Scan Zone";
            } else if (m_deleteZoneType == ZoneType::TargetReferencePoint) {
                success = m_stateModel->deleteTRP(m_editingZoneId);
                zoneTypeName = "TRP";
            } else {
                qWarning() << "Unknown zone type for deletion:"
                           << static_cast<int>(m_deleteZoneType);
                success = false;
                zoneTypeName = "Unknown";
            }

            if (success) {
                // Save to file
                QString zonesPath = QCoreApplication::applicationDirPath() + "/config/zones.json";
                bool saveSuccess = m_stateModel->saveZonesToFile(zonesPath);

                if (saveSuccess) {
                    setupShowMessageUI(
                        QString("%1 deleted and saved successfully!").arg(zoneTypeName));
                    qDebug() << "Successfully deleted and saved" << zoneTypeName
                             << "ID:" << m_editingZoneId << "to" << zonesPath;
                } else {
                    setupShowMessageUI(
                        QString("%1 deleted but failed to save to file!").arg(zoneTypeName));
                    qWarning() << "Deleted" << zoneTypeName << "but failed to save to JSON";
                }

                transitionToState(State::Show_Message);

                // Auto-return to main menu after 2 seconds
                QTimer::singleShot(2000, this,
                                   [this]() { transitionToState(State::Idle_MainMenu); });
            } else {
                setupShowMessageUI(QString("Failed to delete %1!").arg(zoneTypeName));
                transitionToState(State::Show_Message);
            }
        } else if (selectedItem == "No") {
            // Cancel delete
            resetWipData();
            transitionToState(State::Idle_MainMenu);
        }
    }
}

// ============================================================================
// PARAMETER PANEL INPUT ROUTING
// ============================================================================

void ZoneDefinitionController::routeUpToParameterPanel() {
    switch (m_currentState) {
    case State::AreaZone_Edit_Parameters: {
        int currentField = m_areaZoneParamViewModel->activeField();
        int nextField;

        switch (currentField) {
        case AreaZoneParameterViewModel::Field::Enabled:
            nextField = AreaZoneParameterViewModel::Field::CancelButton;
            break;
        case AreaZoneParameterViewModel::Field::Overridable:
            nextField = AreaZoneParameterViewModel::Field::Enabled;
            break;
        case AreaZoneParameterViewModel::Field::ValidateButton:
            nextField = AreaZoneParameterViewModel::Field::Overridable;
            break;
        case AreaZoneParameterViewModel::Field::CancelButton:
            nextField = AreaZoneParameterViewModel::Field::ValidateButton;
            break;
        default:
            nextField = AreaZoneParameterViewModel::Field::Enabled;
            break;
        }

        m_areaZoneParamViewModel->setActiveField(nextField);
        break;
    }

    case State::SectorScan_Edit_Parameters: {
        bool isEditing = m_sectorScanParamViewModel->isEditingValue();

        if (isEditing && m_sectorScanParamViewModel->activeField() ==
                             SectorScanParameterViewModel::Field::ScanSpeed) {
            // Increase scan speed
            int speed = m_sectorScanParamViewModel->scanSpeed();
            if (speed < 10) {
                m_sectorScanParamViewModel->setScanSpeed(speed + 1);
            }
        } else {
            // Navigate fields
            int currentField = m_sectorScanParamViewModel->activeField();
            int nextField;

            switch (currentField) {
            case SectorScanParameterViewModel::Field::Enabled:
                nextField = SectorScanParameterViewModel::Field::CancelButton;
                break;
            case SectorScanParameterViewModel::Field::ScanSpeed:
                nextField = SectorScanParameterViewModel::Field::Enabled;
                break;
            case SectorScanParameterViewModel::Field::ValidateButton:
                nextField = SectorScanParameterViewModel::Field::ScanSpeed;
                break;
            case SectorScanParameterViewModel::Field::CancelButton:
                nextField = SectorScanParameterViewModel::Field::ValidateButton;
                break;
            default:
                nextField = SectorScanParameterViewModel::Field::Enabled;
                break;
            }

            m_sectorScanParamViewModel->setActiveField(nextField);
        }
        break;
    }

    case State::TRP_Edit_Parameters: {
        bool isEditing = m_trpParamViewModel->isEditingValue();
        int currentField = m_trpParamViewModel->activeField();

        if (isEditing) {
            // Adjust values
            switch (currentField) {
            case TRPParameterViewModel::Field::LocationPage: {
                int page = m_trpParamViewModel->locationPage();
                if (page < 200)
                    m_trpParamViewModel->setLocationPage(page + 1);
                break;
            }
            case TRPParameterViewModel::Field::TrpInPage: {
                int trp = m_trpParamViewModel->trpInPage();
                if (trp < 50)
                    m_trpParamViewModel->setTrpInPage(trp + 1);
                break;
            }
            case TRPParameterViewModel::Field::HaltTime: {
                float time = m_trpParamViewModel->haltTime();
                if (time < 60.0f)
                    m_trpParamViewModel->setHaltTime(time + 1.0f);
                break;
            }
            default:
                break;
            }
        } else {
            // Navigate fields
            int nextField;

            switch (currentField) {
            case TRPParameterViewModel::Field::LocationPage:
                nextField = TRPParameterViewModel::Field::CancelButton;
                break;
            case TRPParameterViewModel::Field::TrpInPage:
                nextField = TRPParameterViewModel::Field::LocationPage;
                break;
            case TRPParameterViewModel::Field::HaltTime:
                nextField = TRPParameterViewModel::Field::TrpInPage;
                break;
            case TRPParameterViewModel::Field::ValidateButton:
                nextField = TRPParameterViewModel::Field::HaltTime;
                break;
            case TRPParameterViewModel::Field::CancelButton:
                nextField = TRPParameterViewModel::Field::ValidateButton;
                break;
            default:
                nextField = TRPParameterViewModel::Field::LocationPage;
                break;
            }

            m_trpParamViewModel->setActiveField(nextField);
        }
        break;
    }

    default:
        break;
    }
}

void ZoneDefinitionController::routeDownToParameterPanel() {
    switch (m_currentState) {
    case State::AreaZone_Edit_Parameters: {
        int currentField = m_areaZoneParamViewModel->activeField();
        int nextField;

        switch (currentField) {
        case AreaZoneParameterViewModel::Field::Enabled:
            nextField = AreaZoneParameterViewModel::Field::Overridable;
            break;
        case AreaZoneParameterViewModel::Field::Overridable:
            nextField = AreaZoneParameterViewModel::Field::ValidateButton;
            break;
        case AreaZoneParameterViewModel::Field::ValidateButton:
            nextField = AreaZoneParameterViewModel::Field::CancelButton;
            break;
        case AreaZoneParameterViewModel::Field::CancelButton:
            nextField = AreaZoneParameterViewModel::Field::Enabled;
            break;
        default:
            nextField = AreaZoneParameterViewModel::Field::Enabled;
            break;
        }

        m_areaZoneParamViewModel->setActiveField(nextField);
        break;
    }

    case State::SectorScan_Edit_Parameters: {
        bool isEditing = m_sectorScanParamViewModel->isEditingValue();

        if (isEditing && m_sectorScanParamViewModel->activeField() ==
                             SectorScanParameterViewModel::Field::ScanSpeed) {
            // Decrease scan speed
            int speed = m_sectorScanParamViewModel->scanSpeed();
            if (speed > 1) {
                m_sectorScanParamViewModel->setScanSpeed(speed - 1);
            }
        } else {
            // Navigate fields
            int currentField = m_sectorScanParamViewModel->activeField();
            int nextField;

            switch (currentField) {
            case SectorScanParameterViewModel::Field::Enabled:
                nextField = SectorScanParameterViewModel::Field::ScanSpeed;
                break;
            case SectorScanParameterViewModel::Field::ScanSpeed:
                nextField = SectorScanParameterViewModel::Field::ValidateButton;
                break;
            case SectorScanParameterViewModel::Field::ValidateButton:
                nextField = SectorScanParameterViewModel::Field::CancelButton;
                break;
            case SectorScanParameterViewModel::Field::CancelButton:
                nextField = SectorScanParameterViewModel::Field::Enabled;
                break;
            default:
                nextField = SectorScanParameterViewModel::Field::Enabled;
                break;
            }

            m_sectorScanParamViewModel->setActiveField(nextField);
        }
        break;
    }

    case State::TRP_Edit_Parameters: {
        bool isEditing = m_trpParamViewModel->isEditingValue();
        int currentField = m_trpParamViewModel->activeField();

        if (isEditing) {
            // Adjust values
            switch (currentField) {
            case TRPParameterViewModel::Field::LocationPage: {
                int page = m_trpParamViewModel->locationPage();
                if (page > 1)
                    m_trpParamViewModel->setLocationPage(page - 1);
                break;
            }
            case TRPParameterViewModel::Field::TrpInPage: {
                int trp = m_trpParamViewModel->trpInPage();
                if (trp > 1)
                    m_trpParamViewModel->setTrpInPage(trp - 1);
                break;
            }
            case TRPParameterViewModel::Field::HaltTime: {
                float time = m_trpParamViewModel->haltTime();
                if (time > 1.0f)
                    m_trpParamViewModel->setHaltTime(time - 1.0f);
                break;
            }
            default:
                break;
            }
        } else {
            // Navigate fields
            int nextField;

            switch (currentField) {
            case TRPParameterViewModel::Field::LocationPage:
                nextField = TRPParameterViewModel::Field::TrpInPage;
                break;
            case TRPParameterViewModel::Field::TrpInPage:
                nextField = TRPParameterViewModel::Field::HaltTime;
                break;
            case TRPParameterViewModel::Field::HaltTime:
                nextField = TRPParameterViewModel::Field::ValidateButton;
                break;
            case TRPParameterViewModel::Field::ValidateButton:
                nextField = TRPParameterViewModel::Field::CancelButton;
                break;
            case TRPParameterViewModel::Field::CancelButton:
                nextField = TRPParameterViewModel::Field::LocationPage;
                break;
            default:
                nextField = TRPParameterViewModel::Field::LocationPage;
                break;
            }

            m_trpParamViewModel->setActiveField(nextField);
        }
        break;
    }

    default:
        break;
    }
}

void ZoneDefinitionController::routeSelectToParameterPanel() {
    switch (m_currentState) {
    case State::AreaZone_Edit_Parameters: {
        int currentField = m_areaZoneParamViewModel->activeField();

        switch (currentField) {
        case AreaZoneParameterViewModel::Field::Enabled:
            // Toggle enabled
            m_areaZoneParamViewModel->setIsEnabled(!m_areaZoneParamViewModel->isEnabled());
            break;

        case AreaZoneParameterViewModel::Field::Overridable:
            // Toggle overridable
            m_areaZoneParamViewModel->setIsOverridable(!m_areaZoneParamViewModel->isOverridable());
            break;

        case AreaZoneParameterViewModel::Field::ValidateButton:
            // Validate - save data
            processEditParametersConfirm();
            break;

        case AreaZoneParameterViewModel::Field::CancelButton:
            // Cancel - return to aiming or main menu
            if (m_editingZoneId != -1) {
                // Editing existing zone - reload original data
                if (auto* zone = m_stateModel->getAreaZoneById(m_editingZoneId)) {
                    m_wipAreaZone = *zone;
                }
            }
            resetWipData();
            transitionToState(State::Idle_MainMenu);
            break;

        default:
            break;
        }
        break;
    }

    case State::SectorScan_Edit_Parameters: {
        int currentField = m_sectorScanParamViewModel->activeField();
        bool isEditing = m_sectorScanParamViewModel->isEditingValue();

        switch (currentField) {
        case SectorScanParameterViewModel::Field::Enabled:
            // Toggle enabled
            m_sectorScanParamViewModel->setIsEnabled(!m_sectorScanParamViewModel->isEnabled());
            break;

        case SectorScanParameterViewModel::Field::ScanSpeed:
            // Toggle edit mode
            m_sectorScanParamViewModel->setIsEditingValue(!isEditing);
            break;

        case SectorScanParameterViewModel::Field::ValidateButton:
            // Validate - save data
            processEditParametersConfirm();
            break;

        case SectorScanParameterViewModel::Field::CancelButton:
            // Cancel
            if (m_editingZoneId != -1) {
                if (auto* zone = m_stateModel->getSectorScanZoneById(m_editingZoneId)) {
                    m_wipSectorScan = *zone;
                }
            }
            resetWipData();
            transitionToState(State::Idle_MainMenu);
            break;

        default:
            break;
        }
        break;
    }

    case State::TRP_Edit_Parameters: {
        int currentField = m_trpParamViewModel->activeField();
        bool isEditing = m_trpParamViewModel->isEditingValue();

        switch (currentField) {
        case TRPParameterViewModel::Field::LocationPage:
        case TRPParameterViewModel::Field::TrpInPage:
        case TRPParameterViewModel::Field::HaltTime:
            // Toggle edit mode
            m_trpParamViewModel->setIsEditingValue(!isEditing);
            break;

        case TRPParameterViewModel::Field::ValidateButton:
            // Validate - save data
            processEditParametersConfirm();
            break;

        case TRPParameterViewModel::Field::CancelButton:
            // Cancel
            if (m_editingZoneId != -1) {
                if (auto* trp = m_stateModel->getTRPById(m_editingZoneId)) {
                    m_wipTRP = *trp;
                }
            }
            resetWipData();
            transitionToState(State::Idle_MainMenu);
            break;

        default:
            break;
        }
        break;
    }

    default:
        break;
    }
}

// ============================================================================
// GEOMETRY CALCULATION & MAP UPDATE
// ============================================================================

float ZoneDefinitionController::normalizeAzimuthTo360(float az) const {
    float normalized = fmod(az, 360.0f);
    if (normalized < 0) {
        normalized += 360.0f;
    }
    return normalized;
}

void ZoneDefinitionController::calculateAreaZoneGeometry() {
    float az1_norm = normalizeAzimuthTo360(m_wipAz1);
    float az2_norm = normalizeAzimuthTo360(
        (m_currentState == State::AreaZone_Aim_Corner2) ? m_currentGimbalAz : m_wipAz2);
    float el1 = m_wipEl1;
    float el2 = (m_currentState == State::AreaZone_Aim_Corner2) ? m_currentGimbalEl : m_wipEl2;

    // Determine min/max elevation
    m_wipAreaZone.minElevation = std::min(el1, el2);
    m_wipAreaZone.maxElevation = std::max(el1, el2);

    // Determine start/end azimuth, handling wrap-around
    float diff = az2_norm - az1_norm;
    if (diff >= 0) {
        if (diff <= 180.0f) {
            m_wipAreaZone.startAzimuth = az1_norm;
            m_wipAreaZone.endAzimuth = az2_norm;
        } else {
            m_wipAreaZone.startAzimuth = az2_norm;
            m_wipAreaZone.endAzimuth = az1_norm;
        }
    } else {
        if (diff >= -180.0f) {
            m_wipAreaZone.startAzimuth = az2_norm;
            m_wipAreaZone.endAzimuth = az1_norm;
        } else {
            m_wipAreaZone.startAzimuth = az1_norm;
            m_wipAreaZone.endAzimuth = az2_norm;
        }
    }

    qDebug() << "Calculated AreaZone Geometry: StartAz=" << m_wipAreaZone.startAzimuth
             << "EndAz=" << m_wipAreaZone.endAzimuth << "MinEl=" << m_wipAreaZone.minElevation
             << "MaxEl=" << m_wipAreaZone.maxElevation;
}

void ZoneDefinitionController::updateMapWipZone() {
    QVariantMap wipData;
    bool isDefiningStart = false;
    bool isDefiningEnd = false;
    int wipType = 0;

    switch (m_currentState) {
    case State::AreaZone_Aim_Corner1:
        // Show gimbal position as potential corner 1
        wipData["startAzimuth"] = m_currentGimbalAz;
        wipData["endAzimuth"] = m_currentGimbalAz;
        wipData["minElevation"] = m_currentGimbalEl;
        wipData["maxElevation"] = m_currentGimbalEl;
        wipType = 1;  // AreaZone
        isDefiningStart = true;
        isDefiningEnd = false;
        break;

    case State::AreaZone_Aim_Corner2:
        // Show rectangle defined by corner 1 and current gimbal
        calculateAreaZoneGeometry();
        wipData["startAzimuth"] = m_wipAreaZone.startAzimuth;
        wipData["endAzimuth"] = m_wipAreaZone.endAzimuth;
        wipData["minElevation"] = m_wipAreaZone.minElevation;
        wipData["maxElevation"] = m_wipAreaZone.maxElevation;
        wipType = 1;
        isDefiningStart = true;
        isDefiningEnd = true;
        break;

    case State::AreaZone_Edit_Parameters:
        // Show final defined rectangle
        wipData["startAzimuth"] = m_wipAreaZone.startAzimuth;
        wipData["endAzimuth"] = m_wipAreaZone.endAzimuth;
        wipData["minElevation"] = m_wipAreaZone.minElevation;
        wipData["maxElevation"] = m_wipAreaZone.maxElevation;
        wipType = 1;
        isDefiningStart = true;
        isDefiningEnd = true;
        break;

    case State::SectorScan_Aim_Point1:
        wipData["az1"] = m_currentGimbalAz;
        wipData["el1"] = m_currentGimbalEl;
        wipData["az2"] = m_currentGimbalAz;
        wipData["el2"] = m_currentGimbalEl;
        wipType = 2;  // SectorScan
        isDefiningStart = true;
        isDefiningEnd = false;
        break;

    case State::SectorScan_Aim_Point2:
        wipData["az1"] = m_wipSectorScan.az1;
        wipData["el1"] = m_wipSectorScan.el1;
        wipData["az2"] = m_currentGimbalAz;
        wipData["el2"] = m_currentGimbalEl;
        wipType = 2;
        isDefiningStart = true;
        isDefiningEnd = true;
        break;

    case State::SectorScan_Edit_Parameters:
        wipData["az1"] = m_wipSectorScan.az1;
        wipData["el1"] = m_wipSectorScan.el1;
        wipData["az2"] = m_wipSectorScan.az2;
        wipData["el2"] = m_wipSectorScan.el2;
        wipType = 2;
        isDefiningStart = true;
        isDefiningEnd = true;
        break;

    case State::TRP_Aim_Point:
        wipData["azimuth"] = m_currentGimbalAz;
        wipData["elevation"] = m_currentGimbalEl;
        wipType = 3;  // TRP
        isDefiningStart = true;
        isDefiningEnd = false;
        break;

    case State::TRP_Edit_Parameters:
        wipData["azimuth"] = m_wipTRP.azimuth;
        wipData["elevation"] = m_wipTRP.elevation;
        wipType = 3;
        isDefiningStart = true;
        isDefiningEnd = true;
        break;

    default:
        // Not in an aiming or editing state
        m_mapViewModel->clearWipZone();
        return;
    }

    m_mapViewModel->setWipZone(wipData, wipType, isDefiningStart, isDefiningEnd);
}

// ============================================================================
// DEPENDENCY INJECTION SETTERS
// ============================================================================
void ZoneDefinitionController::setViewModel(ZoneDefinitionViewModel* viewModel) {
    m_viewModel = viewModel;
}
void ZoneDefinitionController::setParameterViewModels(AreaZoneParameterViewModel* areaVM,
                                                      SectorScanParameterViewModel* sectorVM,
                                                      TRPParameterViewModel* trpVM) {
    m_areaZoneParamViewModel = areaVM;
    m_sectorScanParamViewModel = sectorVM;
    m_trpParamViewModel = trpVM;
}
void ZoneDefinitionController::setMapViewModel(ZoneMapViewModel* mapViewModel) {
    m_mapViewModel = mapViewModel;
}
void ZoneDefinitionController::setStateModel(SystemStateModel* stateModel) {
    m_stateModel = stateModel;
}

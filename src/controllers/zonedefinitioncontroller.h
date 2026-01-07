#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>
#include "models/domain/systemstatedata.h"

class ZoneDefinitionViewModel;
class ZoneMapViewModel;
class AreaZoneParameterViewModel;
class SectorScanParameterViewModel;
class TRPParameterViewModel;
class SystemStateModel;

/**
 * @brief Controller for Zone Definition workflow
 * Implements the state machine for zone creation/editing/deletion
 */
class ZoneDefinitionController : public QObject {
    Q_OBJECT

public:
    // State machine - matches ZoneDefinitionWidget exactly
    enum class State {
        Idle_MainMenu,
        Select_ZoneType_ForNew,
        Select_ZoneType_ForModify,
        Select_ZoneType_ForDelete,

        // AreaZone flow
        Select_AreaZone_ToModify,
        Select_AreaZone_ToDelete,
        AreaZone_Aim_Corner1,
        AreaZone_Aim_Corner2,
        AreaZone_Edit_Parameters,

        // SectorScan flow
        Select_SectorScan_ToModify,
        Select_SectorScan_ToDelete,
        SectorScan_Aim_Point1,
        SectorScan_Aim_Point2,
        SectorScan_Edit_Parameters,

        // TRP flow
        Select_TRP_ToModify,
        Select_TRP_ToDelete,
        TRP_Aim_Point,
        TRP_Edit_Parameters,

        // Common
        Confirm_Save,
        Confirm_Delete,
        Show_Message
    };
    Q_ENUM(State)

    explicit ZoneDefinitionController(QObject* parent = nullptr);
    void initialize();
    void setViewModel(ZoneDefinitionViewModel* viewModel);
    void setParameterViewModels(AreaZoneParameterViewModel* areaVM,
                                SectorScanParameterViewModel* sectorVM,
                                TRPParameterViewModel* trpVM);
    void setMapViewModel(ZoneMapViewModel* mapViewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    void show();
    void hide();

    // 3-button input
    void onUpButtonPressed();
    void onDownButtonPressed();
    void onMenuValButtonPressed();

signals:
    void closed();
    void returnToMainMenu();

private slots:
    // Model updates
    void onGimbalPositionChanged(float az, float el);
    void onZonesChanged();
    void onColorStyleChanged(const QColor& style);

    // State-specific actions
    void processMainMenuSelect();
    void processSelectZoneTypeSelect();
    void processSelectZoneTypeForModifyDeleteSelect();
    void processSelectExistingZoneSelect();
    void processAimPointConfirm();
    void processEditParametersConfirm();
    void processConfirmSaveSelect();
    void processConfirmDeleteSelect();

private:
    void transitionToState(State newState);
    void updateUI();
    void resetWipData();

    // UI setup helpers
    void setupIdleMainMenuUI();
    void setupSelectZoneTypeUI();
    void setupSelectZoneTypeForModifyDeleteUI(const QString& action);
    void setupSelectExistingZoneUI(ZoneType typeToSelect, const QString& title);
    void setupAimPointUI(const QString& instructionText);
    void setupAreaZoneParametersUI(bool isNew);
    void setupSectorScanParametersUI(bool isNew);
    void setupTRPParametersUI(bool isNew);
    void setupConfirmUI(const QString& title, const QString& question);
    void setupShowMessageUI(const QString& message);

    // Parameter panel input routing
    void routeUpToParameterPanel();
    void routeDownToParameterPanel();
    void routeSelectToParameterPanel();

    // AreaZone geometry calculation
    void calculateAreaZoneGeometry();
    float normalizeAzimuthTo360(float az) const;

    // WIP zone map update
    void updateMapWipZone();
    State m_currentState;

    // ViewModels
    ZoneDefinitionViewModel* m_viewModel;
    ZoneMapViewModel* m_mapViewModel;
    AreaZoneParameterViewModel* m_areaZoneParamViewModel;
    SectorScanParameterViewModel* m_sectorScanParamViewModel;
    TRPParameterViewModel* m_trpParamViewModel;

    // Models
    SystemStateModel* m_stateModel;

    // WIP data
    int m_editingZoneId;
    ZoneType m_wipZoneType;
    ZoneType m_deleteZoneType;  // Store type for deletion confirmation
    AreaZone m_wipAreaZone;
    AutoSectorScanZone m_wipSectorScan;
    TargetReferencePoint m_wipTRP;

    // AreaZone aiming state
    bool m_corner1Defined;
    float m_wipAz1, m_wipEl1, m_wipAz2, m_wipEl2;

    // Current gimbal position
    float m_currentGimbalAz;
    float m_currentGimbalEl;

    // Menu navigation
    QStringList m_currentMenuItems;
    int m_currentMenuIndex;
};



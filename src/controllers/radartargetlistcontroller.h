#ifndef RADARTARGETLISTCONTROLLER_H
#define RADARTARGETLISTCONTROLLER_H

#include <QObject>
#include <QStringList>
#include "models/domain/systemstatedata.h"

class SystemStateModel;
class RadarTargetListViewModel;

/**
 * @brief RadarTargetListController - Manages radar target selection list
 *
 * When radar motion mode is selected, this controller:
 * - Displays a list of radar targets under the STATUS BLOCK
 * - Allows scrolling through targets with UP/DOWN buttons
 * - First row is empty (no selection by default)
 * - Validates selection to trigger gimbal slew to target
 * - Clears selection when exiting radar mode
 *
 * Based on MainMenuController pattern for button handling.
 */
class RadarTargetListController : public QObject
{
    Q_OBJECT

public:
    explicit RadarTargetListController(QObject *parent = nullptr);

    void initialize();
    void setViewModel(RadarTargetListViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    // Show/hide the radar target list
    void show();
    void hide();

    // Input handling - 3 buttons (UP, DOWN, MENU/VAL)
    void onUpButtonPressed();
    void onDownButtonPressed();
    void onSelectButtonPressed();  // Called by MENU/VAL when in radar targets state

    // Called when radar mode is entered/exited
    void onRadarModeEntered();
    void onRadarModeExited();

signals:
    // Events for ApplicationController
    void targetValidated(quint32 targetId);  // Target selected and validated
    void listClosed();                        // User cancelled or exited
    void returnToMainMenu();
    void radarListShown();                    // Radar list became visible (for state management)
    void radarListHidden();                   // Radar list became hidden (for state management)

private slots:
    // Monitor radar plots changes
    void onSystemStateChanged(const SystemStateData& newData);

private:
    void updateListFromRadarPlots();
    void updateViewModelSelection();
    void clearSelection();

    RadarTargetListViewModel* m_viewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;

    // Current radar plots cache
    QVector<SimpleRadarPlot> m_radarPlots;

    // Selection state
    // Index 0 = empty row (no selection)
    // Index 1..N = actual radar targets
    int m_currentIndex = 0;

    // Track if list is visible
    bool m_isVisible = false;

    // Track previous motion mode for transition detection
    // (only show list on TRANSITION to RadarSlew, not while staying in it)
    MotionMode m_previousMotionMode = MotionMode::Idle;
};

#endif // RADARTARGETLISTCONTROLLER_H

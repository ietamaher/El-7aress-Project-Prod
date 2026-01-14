#include "controllers/radartargetlistcontroller.h"
#include "models/radartargetlistviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

RadarTargetListController::RadarTargetListController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_stateModel(nullptr)
    , m_currentIndex(0)
    , m_isVisible(false)
{
}

void RadarTargetListController::initialize()
{
    qDebug() << "RadarTargetListController: Initializing...";

    // Verify dependencies
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_stateModel);

    // Connect to state changes to update radar plots
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, &RadarTargetListController::onSystemStateChanged);

    qDebug() << "RadarTargetListController: Initialized";
}

void RadarTargetListController::setViewModel(RadarTargetListViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void RadarTargetListController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}

// ============================================================================
// SHOW / HIDE
// ============================================================================

void RadarTargetListController::show()
{
    qDebug() << "RadarTargetListController: show()";

    m_isVisible = true;

    // Load current radar plots
    updateListFromRadarPlots();

    // Reset selection to first row (empty/no selection)
    m_currentIndex = 0;
    updateViewModelSelection();

    if (m_viewModel) {
        m_viewModel->setVisible(true);
    }

    // Notify ApplicationController to route button presses to us
    emit radarListShown();
}

void RadarTargetListController::hide()
{
    qDebug() << "RadarTargetListController: hide()";

    m_isVisible = false;

    if (m_viewModel) {
        m_viewModel->setVisible(false);
    }

    // Notify ApplicationController to stop routing button presses to us
    emit radarListHidden();
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void RadarTargetListController::onUpButtonPressed()
{
    if (!m_isVisible) return;

    qDebug() << "RadarTargetListController: UP pressed, current index:" << m_currentIndex;

    // Scroll up with wraparound
    // Total items = 1 (empty row) + radar plots count
    int totalItems = 1 + m_radarPlots.size();

    if (totalItems > 0) {
        m_currentIndex--;
        if (m_currentIndex < 0) {
            m_currentIndex = totalItems - 1;  // Wrap to bottom
        }
        updateViewModelSelection();
    }

    qDebug() << "RadarTargetListController: New index:" << m_currentIndex;
}

void RadarTargetListController::onDownButtonPressed()
{
    if (!m_isVisible) return;

    qDebug() << "RadarTargetListController: DOWN pressed, current index:" << m_currentIndex;

    // Scroll down with wraparound
    int totalItems = 1 + m_radarPlots.size();

    if (totalItems > 0) {
        m_currentIndex++;
        if (m_currentIndex >= totalItems) {
            m_currentIndex = 0;  // Wrap to top
        }
        updateViewModelSelection();
    }

    qDebug() << "RadarTargetListController: New index:" << m_currentIndex;
}

void RadarTargetListController::onSelectButtonPressed()
{
    if (!m_isVisible) return;

    qDebug() << "RadarTargetListController: SELECT pressed at index:" << m_currentIndex;

    // Index 0 = empty row (cancel/no selection)
    if (m_currentIndex == 0) {
        qDebug() << "RadarTargetListController: Empty row selected - closing list";
        clearSelection();
        hide();
        emit listClosed();
        return;
    }

    // Get the selected radar target (index - 1 because first row is empty)
    int plotIndex = m_currentIndex - 1;
    if (plotIndex >= 0 && plotIndex < m_radarPlots.size()) {
        const SimpleRadarPlot& selectedTarget = m_radarPlots[plotIndex];

        qInfo() << "RadarTargetListController: Target validated - ID:" << selectedTarget.id
                << "Az:" << selectedTarget.azimuth << "Range:" << selectedTarget.range;

        // Set the selected target ID in the state model
        // This will trigger RadarSlewMotionMode to start slewing
        if (m_stateModel) {
            m_stateModel->setSelectedRadarTrackId(selectedTarget.id);
        }

        emit targetValidated(selectedTarget.id);

        // Hide the list after selection
        hide();
    } else {
        qWarning() << "RadarTargetListController: Invalid plot index:" << plotIndex;
    }
}

// ============================================================================
// RADAR MODE ENTER/EXIT
// ============================================================================

void RadarTargetListController::onRadarModeEntered()
{
    qDebug() << "RadarTargetListController: Radar mode entered";

    // Populate with dummy data for testing
    // In production, this would come from actual radar data
    if (m_stateModel) {
        QVector<SimpleRadarPlot> dummyPlots;
        dummyPlots.append({101, 45.0f, 1500.0f, 180.0f, 0.0f});   // ID 101, NE quadrant, 1.5km
        dummyPlots.append({102, 110.0f, 850.0f, 290.0f, 5.0f});   // ID 102, SE quadrant, 850m
        dummyPlots.append({103, 315.0f, 2200.0f, 120.0f, 15.0f}); // ID 103, NW quadrant, 2.2km
        dummyPlots.append({104, 260.0f, 500.0f, 80.0f, 25.0f});   // ID 104, SW quadrant, 500m
        dummyPlots.append({105, 5.0f, 3100.0f, 175.0f, -2.0f});   // ID 105, Directly ahead, 3.1km
        dummyPlots.append({106, 178.0f, 4500.0f, 0.0f, 2.0f});    // ID 106, Directly behind, 4.5km

        // Update the state model with dummy radar plots
        SystemStateData currentData = m_stateModel->data();
        currentData.radarPlots = dummyPlots;
        currentData.selectedRadarTrackId = 0;  // Clear any previous selection
        m_stateModel->updateData(currentData);
    }

    show();
}

void RadarTargetListController::onRadarModeExited()
{
    qDebug() << "RadarTargetListController: Radar mode exited";

    clearSelection();
    hide();
}

// ============================================================================
// STATE CHANGE HANDLER
// ============================================================================

void RadarTargetListController::onSystemStateChanged(const SystemStateData& newData)
{
    // Detect motion mode TRANSITIONS to/from RadarSlew
    // Only trigger on actual mode change, not on every state update while in the mode
    MotionMode currentMode = newData.motionMode;

    if (currentMode == MotionMode::RadarSlew && m_previousMotionMode != MotionMode::RadarSlew) {
        // TRANSITION: Just entered RadarSlew mode - show target list
        qDebug() << "RadarTargetListController: Transitioned TO RadarSlew mode - showing target list";
        onRadarModeEntered();
    } else if (currentMode != MotionMode::RadarSlew && m_previousMotionMode == MotionMode::RadarSlew) {
        // TRANSITION: Just exited RadarSlew mode - hide target list
        qDebug() << "RadarTargetListController: Transitioned FROM RadarSlew mode - hiding target list";
        onRadarModeExited();
    }

    // Update previous mode for next comparison
    m_previousMotionMode = currentMode;

    // Update radar plots if visible and changed
    if (m_isVisible && newData.radarPlots != m_radarPlots) {
        updateListFromRadarPlots();
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void RadarTargetListController::updateListFromRadarPlots()
{
    if (!m_stateModel) return;

    m_radarPlots = m_stateModel->data().radarPlots;

    qDebug() << "RadarTargetListController: Loaded" << m_radarPlots.size() << "radar plots";

    // Build display list for view model
    // First item is empty row "--- SELECT TARGET ---"
    QStringList displayList;
    displayList.append("--- SELECT TARGET ---");

    for (const auto& plot : m_radarPlots) {
        // Format: "ID: 101 | Az: 45.0° | Range: 1500m"
        QString entry = QString("ID: %1 | Az: %2° | R: %3m")
                            .arg(plot.id)
                            .arg(plot.azimuth, 0, 'f', 1)
                            .arg(plot.range, 0, 'f', 0);
        displayList.append(entry);
    }

    if (m_viewModel) {
        m_viewModel->setTargetList(displayList);
    }

    // Ensure current index is still valid
    int totalItems = 1 + m_radarPlots.size();
    if (m_currentIndex >= totalItems) {
        m_currentIndex = 0;
    }

    updateViewModelSelection();
}

void RadarTargetListController::updateViewModelSelection()
{
    if (m_viewModel) {
        m_viewModel->setCurrentIndex(m_currentIndex);
    }
}

void RadarTargetListController::clearSelection()
{
    qDebug() << "RadarTargetListController: Clearing selection";

    m_currentIndex = 0;

    // Clear the selected target ID in state model
    if (m_stateModel) {
        m_stateModel->setSelectedRadarTrackId(0);
    }

    updateViewModelSelection();
}

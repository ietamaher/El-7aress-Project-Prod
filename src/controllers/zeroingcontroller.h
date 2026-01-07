#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
// ============================================================================
// Qt Framework
// ============================================================================
#include <QObject>
#include <QColor>

// ============================================================================
// Forward Declarations
// ============================================================================
class ZeroingViewModel;
class SystemStateModel;

/**
 * @brief Weapon zeroing procedure controller
 *
 * This class manages the zeroing procedure workflow:
 * 1. User fires weapon at fixed target
 * 2. User moves reticle to actual impact point
 * 3. System captures offset as zeroing correction
 *
 * Zeroing offsets are applied to gimbal pointing to compensate for
 * mechanical alignment errors between sight line and weapon bore.
 */
class ZeroingController : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // Constructor
    // ============================================================================
    explicit ZeroingController(QObject* parent = nullptr);

    // ============================================================================
    // Initialization
    // ============================================================================
    void initialize();
    void setViewModel(ZeroingViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    // ============================================================================
    // UI Control
    // ============================================================================
    void show();
    void hide();

    // ============================================================================
    // Button Handlers
    // ============================================================================
    void onSelectButtonPressed();
    void onBackButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();

signals:
    // ============================================================================
    // Navigation Signals
    // ============================================================================
    void zeroingFinished();
    void returnToMainMenu();

private slots:
    // ============================================================================
    // State Handlers
    // ============================================================================
    void onColorStyleChanged(const QColor& color);

private:
    // ============================================================================
    // State Machine
    // ============================================================================
    enum class ZeroingState { Idle, Instruct_MoveReticleToImpact, Completed };

    void transitionToState(ZeroingState newState);
    void updateUI();

    // ============================================================================
    // Dependencies
    // ============================================================================
    ZeroingViewModel* m_viewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;

    // ============================================================================
    // State
    // ============================================================================
    ZeroingState m_currentState = ZeroingState::Idle;
};


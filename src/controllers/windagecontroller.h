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
class WindageViewModel;
class SystemStateModel;

/**
 * @brief Wind compensation procedure controller
 *
 * This class manages the windage setup workflow:
 * 1. User aligns weapon station towards the wind direction
 * 2. System captures absolute wind direction (IMU yaw + station azimuth)
 * 3. User sets wind speed in knots
 * 4. System calculates crosswind component for ballistic corrections
 *
 * Wind direction is stored as absolute bearing (relative to true North)
 * so corrections remain valid even if vehicle rotates.
 */
class WindageController : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // Constructor
    // ============================================================================
    explicit WindageController(QObject* parent = nullptr);

    // ============================================================================
    // Initialization
    // ============================================================================
    void initialize();
    void setViewModel(WindageViewModel* viewModel);
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
    void windageFinished();
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
    enum class WindageState { Idle, Instruct_AlignToWind, Set_WindSpeed, Completed };

    void transitionToState(WindageState newState);
    void updateUI();

    // ============================================================================
    // Dependencies
    // ============================================================================
    WindageViewModel* m_viewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;

    // ============================================================================
    // State
    // ============================================================================
    WindageState m_currentState = WindageState::Idle;
    float m_currentWindSpeedEdit = 0.0f;
};


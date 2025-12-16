#ifndef HOMECALIBRATIONCONTROLLER_H
#define HOMECALIBRATIONCONTROLLER_H

/**
 * @file HomeCalibrationController.h
 * @brief Controller for the Azimuth Home Offset Calibration procedure
 *
 * This controller manages the workflow for calibrating the azimuth home position
 * to compensate for encoder drift in the Oriental Motor AZD-KD with ABZO encoder.
 *
 * Calibration Procedure:
 * 1. State: Instruct_GoToEncoderHome
 *    - System displays encoder's current "home" position (may be drifted)
 *    - Operator observes that gimbal is NOT at true mechanical home
 *    - Press MENU/VAL to proceed
 *
 * 2. State: Instruct_SlewToTrueHome
 *    - Operator manually slews gimbal to visual/mechanical alignment mark
 *    - Current encoder position and calculated offset displayed in real-time
 *    - Press MENU/VAL to capture this position as "true home"
 *
 * 3. State: Completed
 *    - Shows final offset applied
 *    - Offset saved to persistent storage
 *    - Press MENU/VAL to return to main menu
 *
 * @author Claude Code
 * @date December 2025
 */

#include <QObject>
#include <QColor>
#include <QTimer>

// Forward Declarations
class HomeCalibrationViewModel;
class SystemStateModel;

class HomeCalibrationController : public QObject
{
    Q_OBJECT

public:
    explicit HomeCalibrationController(QObject* parent = nullptr);

    // ========================================================================
    // Initialization
    // ========================================================================
    void initialize();
    void setViewModel(HomeCalibrationViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    // ========================================================================
    // UI Control
    // ========================================================================
    void show();
    void hide();

    // ========================================================================
    // Button Handlers
    // ========================================================================
    void onSelectButtonPressed();
    void onBackButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();

signals:
    // ========================================================================
    // Navigation Signals
    // ========================================================================
    void calibrationFinished();
    void returnToMainMenu();

private slots:
    // ========================================================================
    // State Handlers
    // ========================================================================
    void onColorStyleChanged(const QColor& color);
    void onPositionUpdate();

private:
    // ========================================================================
    // State Machine
    // ========================================================================
    enum class CalibrationState {
        Idle,
        Instruct_GoToEncoderHome,
        Instruct_SlewToTrueHome,
        Completed
    };

    void transitionToState(CalibrationState newState);
    void updateUI();

    // ========================================================================
    // Dependencies
    // ========================================================================
    HomeCalibrationViewModel* m_viewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;

    // ========================================================================
    // State
    // ========================================================================
    CalibrationState m_currentState = CalibrationState::Idle;

    // Timer for updating position display during calibration
    QTimer* m_updateTimer = nullptr;
    static constexpr int POSITION_UPDATE_INTERVAL_MS = 100;
};

#endif // HOMECALIBRATIONCONTROLLER_H

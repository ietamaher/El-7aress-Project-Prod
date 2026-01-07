#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>

class EnvironmentalViewModel;
class SystemStateModel;

class EnvironmentalController : public QObject {
    Q_OBJECT

public:
    explicit EnvironmentalController(QObject* parent = nullptr);
    void initialize();
    void setViewModel(EnvironmentalViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    void show();
    void hide();
    void onSelectButtonPressed();
    void onBackButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();

signals:
    void environmentalFinished();
    void returnToMainMenu();

private slots:
    void onColorStyleChanged(const QColor& color);

private:
    enum class EnvironmentalState { Idle, Set_Temperature, Set_Altitude, Completed };

    void updateUI();
    void transitionToState(EnvironmentalState newState);

    EnvironmentalViewModel* m_viewModel;
    SystemStateModel* m_stateModel;
    EnvironmentalState m_currentState;
    float m_currentTemperatureEdit;
    float m_currentAltitudeEdit;
    // NOTE: Crosswind removed - use Windage menu instead (wind direction + speed)
};



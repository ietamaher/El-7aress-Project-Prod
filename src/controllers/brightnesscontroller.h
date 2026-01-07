#ifndef BRIGHTNESSCONTROLLER_H
#define BRIGHTNESSCONTROLLER_H

#include <QObject>
#include <QProcess>

class BrightnessViewModel;
class SystemStateModel;

/**
 * @class BrightnessController
 * @brief Controller for display brightness adjustment.
 *
 * Provides simple brightness control using xrandr.
 * Brightness values range from 10% to 100% in 10% steps.
 * Default value is 70%.
 */
class BrightnessController : public QObject {
    Q_OBJECT

public:
    explicit BrightnessController(QObject* parent = nullptr);
    void initialize();
    void setViewModel(BrightnessViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    void show();
    void hide();
    void onSelectButtonPressed();
    void onBackButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();

signals:
    void brightnessFinished();
    void returnToMainMenu();

private slots:
    void onColorStyleChanged(const QColor& color);

private:
    enum class BrightnessState { Idle, Adjusting, Applied };

    void updateUI();
    void transitionToState(BrightnessState newState);
    void applyBrightness(int percentage);
    QString detectDisplayOutput();

    BrightnessViewModel* m_viewModel;
    SystemStateModel* m_stateModel;
    BrightnessState m_currentState;
    int m_currentBrightnessEdit;
    int m_initialBrightness;
    QString m_displayOutput;

    static constexpr int MIN_BRIGHTNESS = 10;
    static constexpr int MAX_BRIGHTNESS = 100;
    static constexpr int BRIGHTNESS_STEP = 10;
    static constexpr int DEFAULT_BRIGHTNESS = 70;
};

#endif  // BRIGHTNESSCONTROLLER_H

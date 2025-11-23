#ifndef MAINMENUCONTROLLER_H
#define MAINMENUCONTROLLER_H

#include <QObject>

class MenuViewModel;
class SystemStateModel;
struct SystemStateData;

class MainMenuController : public QObject
{
    Q_OBJECT
public:
    explicit MainMenuController(QObject *parent = nullptr);
    void initialize();
    void setViewModel(MenuViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    // API for the root controller to use
    void show();
    void hide();

    // Input handling - Only 3 buttons now
    void onUpButtonPressed();
    void onDownButtonPressed();
    void onSelectButtonPressed();  // Called by MENU/VAL when in main menu

signals:
    // Events for the root controller to listen to
    void personalizeReticleRequested();
    void personalizeColorsRequested();
    void zeroingRequested();
    void clearZeroRequested();
    void windageRequested();
    void clearWindageRequested();
    void environmentalRequested();
    void clearEnvironmentalRequested();
    void zoneDefinitionsRequested();
    void systemStatusRequested();
    void toggleDetectionRequested();
    void shutdownSystemRequested();
    void helpAboutRequested();
    void radarTargetListRequested();

    void menuFinished();

private slots:
    void handleMenuOptionSelected(const QString& option);
    void onColorStyleChanged(const QColor& color);
    void onSystemStateChanged(std::shared_ptr<const SystemStateData> newData);

private:
    MenuViewModel* m_viewModel;
    SystemStateModel* m_stateModel;

    // Track previous state to detect changes
    bool m_previousCameraIsDay = true;
    bool m_previousDetectionEnabled = false;

    // Helper to build menu options
    QStringList buildMainMenuOptions() const;
};

#endif // MAINMENUCONTROLLER_H

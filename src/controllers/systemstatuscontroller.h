#ifndef SYSTEMSTATUSCONTROLLER_H
#define SYSTEMSTATUSCONTROLLER_H

#include <QObject>

class SystemStatusViewModel;
class SystemStateModel;
class SystemStateData;

class SystemStatusController : public QObject
{
    Q_OBJECT

public:
    explicit SystemStatusController(QObject *parent = nullptr);

    void setViewModel(SystemStatusViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);
    void initialize();

    void show();
    void hide();

public slots:
    // Button handlers
    void onSelectButtonPressed();
    void onBackButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();

signals:
    void returnToMainMenu();
    void menuFinished();
    void clearAlarmsSignal();

private slots:
    void onSystemStateChanged(std::shared_ptr<const SystemStateData> data);
    void onClearAlarmsRequested();
    void onColorStyleChanged(const QColor& color);

private:
    QStringList buildAlarmsList(std::shared_ptr<const SystemStateData> data);
    void updateUI();

    SystemStatusViewModel* m_viewModel;
    SystemStateModel* m_stateModel;
};

#endif // SYSTEMSTATUSCONTROLLER_H

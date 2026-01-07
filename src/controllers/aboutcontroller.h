#ifndef ABOUTCONTROLLER_H
#define ABOUTCONTROLLER_H

#include <QObject>

class AboutViewModel;
class SystemStateModel;

/**
 * @brief Controller for the About/Help dialog
 *
 * Manages the Help/About information screen, displaying application
 * version, credits, copyright, and system information.
 */
class AboutController : public QObject {
    Q_OBJECT

public:
    explicit AboutController(QObject* parent = nullptr);
    ~AboutController();

    // Dependency injection
    void setViewModel(AboutViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

    // Initialization
    void initialize();

    // View control
    void show();
    void hide();

public slots:
    // Button handlers
    void onSelectButtonPressed();
    void onBackButtonPressed();
    void onUpButtonPressed();
    void onDownButtonPressed();

signals:
    /**
     * @brief Emitted when the About dialog is closed
     *
     * This signals to ApplicationController that the user has finished
     * viewing the About information and the dialog should be dismissed.
     */
    void aboutFinished();
    void returnToMainMenu();

private slots:
    void onColorStyleChanged(const QColor& color);

private:
    AboutViewModel* m_viewModel;
    SystemStateModel* m_stateModel;
};

#endif  // ABOUTCONTROLLER_H

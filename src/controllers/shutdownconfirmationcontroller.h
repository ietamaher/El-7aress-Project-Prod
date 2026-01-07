#ifndef SHUTDOWNCONFIRMATIONCONTROLLER_H
#define SHUTDOWNCONFIRMATIONCONTROLLER_H

#include <QObject>
#include <QTimer>

class ShutdownConfirmationViewModel;
class SystemStateModel;

/**
 * @brief Controller for the shutdown confirmation dialog
 *
 * Manages the shutdown confirmation flow:
 * 1. show() - Displays confirmation dialog with YES/NO options (NO selected by default)
 * 2. UP/DOWN - Navigate between YES and NO options
 * 3. MENU/VAL - Confirm selection
 *    - YES: Shows "SHUTDOWN COMPLETE" message, then triggers actual shutdown
 *    - NO: Returns to main menu
 *
 * Safety: Default selection is NO to prevent accidental shutdown.
 */
class ShutdownConfirmationController : public QObject {
    Q_OBJECT

public:
    explicit ShutdownConfirmationController(QObject* parent = nullptr);
    ~ShutdownConfirmationController();

    // Dependency injection
    void setViewModel(ShutdownConfirmationViewModel* viewModel);
    void setStateModel(SystemStateModel* stateModel);

    // Initialization
    void initialize();

    // View control
    void show();
    void hide();

public slots:
    // Button handlers
    void onUpButtonPressed();
    void onDownButtonPressed();
    void onSelectButtonPressed();

signals:
    /**
     * @brief Emitted when user confirms YES - proceed with shutdown
     */
    void shutdownConfirmed();

    /**
     * @brief Emitted when user selects NO or dialog is cancelled
     */
    void shutdownCancelled();

    /**
     * @brief Emitted when the dialog is closed (either way)
     */
    void dialogFinished();

    /**
     * @brief Emitted to return to main menu
     */
    void returnToMainMenu();

private slots:
    void onColorStyleChanged(const QColor& color);
    void performShutdown();

private:
    ShutdownConfirmationViewModel* m_viewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;
    QTimer* m_shutdownTimer = nullptr;

    bool m_shutdownInProgress = false;
};

#endif  // SHUTDOWNCONFIRMATIONCONTROLLER_H

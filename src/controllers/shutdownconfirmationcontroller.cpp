#include "shutdownconfirmationcontroller.h"
#include "models/shutdownconfirmationviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>

ShutdownConfirmationController::ShutdownConfirmationController(QObject *parent)
    : QObject(parent)
    , m_shutdownTimer(new QTimer(this))
{
    qDebug() << "ShutdownConfirmationController: Created";

    // Configure shutdown timer - single shot, 2 second delay to show message
    m_shutdownTimer->setSingleShot(true);
    m_shutdownTimer->setInterval(2000);  // 2 seconds to display "SHUTDOWN COMPLETE"
    connect(m_shutdownTimer, &QTimer::timeout, this, &ShutdownConfirmationController::performShutdown);
}

ShutdownConfirmationController::~ShutdownConfirmationController()
{
    qDebug() << "ShutdownConfirmationController: Destroyed";
}

void ShutdownConfirmationController::setViewModel(ShutdownConfirmationViewModel* viewModel)
{
    m_viewModel = viewModel;
    qDebug() << "ShutdownConfirmationController: ViewModel set";
}

void ShutdownConfirmationController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
    qDebug() << "ShutdownConfirmationController: StateModel set";
}

void ShutdownConfirmationController::initialize()
{
    qDebug() << "ShutdownConfirmationController::initialize()";

    if (!m_viewModel) {
        qCritical() << "ShutdownConfirmationController: ViewModel is null!";
        return;
    }

    if (!m_stateModel) {
        qCritical() << "ShutdownConfirmationController: StateModel is null!";
        return;
    }

    // Connect to color style changes
    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &ShutdownConfirmationController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);

    qDebug() << "ShutdownConfirmationController initialized successfully";
}

void ShutdownConfirmationController::show()
{
    if (m_viewModel) {
        m_shutdownInProgress = false;
        m_viewModel->reset();  // Reset to default state (NO selected)
        m_viewModel->setVisible(true);
        qDebug() << "ShutdownConfirmationController: Showing confirmation dialog";
    }
}

void ShutdownConfirmationController::hide()
{
    if (m_viewModel) {
        m_viewModel->setVisible(false);
        qDebug() << "ShutdownConfirmationController: Hiding confirmation dialog";
    }
}

void ShutdownConfirmationController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "ShutdownConfirmationController: Color changed to" << color;
    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
}

void ShutdownConfirmationController::onUpButtonPressed()
{
    if (!m_viewModel || m_shutdownInProgress) return;

    // Move selection up (YES is 0, NO is 1)
    int current = m_viewModel->selectedOption();
    if (current > ShutdownConfirmationViewModel::YesShutdown) {
        m_viewModel->setSelectedOption(current - 1);
    }
    qDebug() << "ShutdownConfirmationController: UP pressed, selection now:"
             << (m_viewModel->selectedOption() == ShutdownConfirmationViewModel::YesShutdown ? "YES" : "NO");
}

void ShutdownConfirmationController::onDownButtonPressed()
{
    if (!m_viewModel || m_shutdownInProgress) return;

    // Move selection down (YES is 0, NO is 1)
    int current = m_viewModel->selectedOption();
    if (current < ShutdownConfirmationViewModel::NoCancel) {
        m_viewModel->setSelectedOption(current + 1);
    }
    qDebug() << "ShutdownConfirmationController: DOWN pressed, selection now:"
             << (m_viewModel->selectedOption() == ShutdownConfirmationViewModel::YesShutdown ? "YES" : "NO");
}

void ShutdownConfirmationController::onSelectButtonPressed()
{
    if (!m_viewModel || m_shutdownInProgress) return;

    int selected = m_viewModel->selectedOption();
    qDebug() << "ShutdownConfirmationController: Select pressed with option"
             << (selected == ShutdownConfirmationViewModel::YesShutdown ? "YES" : "NO");

    if (selected == ShutdownConfirmationViewModel::YesShutdown) {
        // User confirmed shutdown
        m_shutdownInProgress = true;
        m_viewModel->setStatusMessage("SHUTDOWN COMPLETE");
        qInfo() << "ShutdownConfirmationController: Shutdown confirmed, initiating shutdown sequence...";

        // Start timer to show message before actual shutdown
        m_shutdownTimer->start();
        emit shutdownConfirmed();
    } else {
        // User cancelled
        qInfo() << "ShutdownConfirmationController: Shutdown cancelled by user";
        hide();
        emit shutdownCancelled();
        emit returnToMainMenu();
        emit dialogFinished();
    }
}

void ShutdownConfirmationController::performShutdown()
{
    qInfo() << "ShutdownConfirmationController: Performing system shutdown...";

    hide();
    emit dialogFinished();

    // Sync filesystem first
    QProcess::execute("sync");

    // Use systemctl for service-based deployment
    bool started = QProcess::startDetached("systemctl", QStringList() << "poweroff");
    
    if (!started) {
        qCritical() << "Failed to initiate system poweroff via systemctl!";
        // Fallback
        QProcess::startDetached("sudo", QStringList() << "shutdown" << "-h" << "now");
    }

    QCoreApplication::quit();
}

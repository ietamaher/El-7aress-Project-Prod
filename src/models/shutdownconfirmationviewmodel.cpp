#include "shutdownconfirmationviewmodel.h"
#include <QDebug>

ShutdownConfirmationViewModel::ShutdownConfirmationViewModel(QObject *parent)
    : QObject(parent)
{
    qDebug() << "ShutdownConfirmationViewModel: Created";
}

void ShutdownConfirmationViewModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
        qDebug() << "ShutdownConfirmationViewModel: Visibility changed to" << visible;
    }
}

void ShutdownConfirmationViewModel::setSelectedOption(int option)
{
    // Clamp to valid range
    if (option < YesShutdown) option = YesShutdown;
    if (option > NoCancel) option = NoCancel;

    if (m_selectedOption != option) {
        m_selectedOption = option;
        emit selectedOptionChanged();
        qDebug() << "ShutdownConfirmationViewModel: Selected option changed to"
                 << (option == YesShutdown ? "YES" : "NO");
    }
}

void ShutdownConfirmationViewModel::setStatusMessage(const QString& message)
{
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
        qDebug() << "ShutdownConfirmationViewModel: Status message:" << message;
    }
}

void ShutdownConfirmationViewModel::setAccentColor(const QColor& color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}

void ShutdownConfirmationViewModel::reset()
{
    // Reset to safe defaults when showing the dialog
    m_selectedOption = NoCancel;  // Default to NO for safety
    m_statusMessage.clear();
    emit selectedOptionChanged();
    emit statusMessageChanged();
    qDebug() << "ShutdownConfirmationViewModel: Reset to default state";
}

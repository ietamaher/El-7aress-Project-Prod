#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>
#include <QString>
#include <QColor>

/**
 * @brief ShutdownConfirmationViewModel - ViewModel for shutdown confirmation dialog
 *
 * Provides a confirmation dialog with YES/NO options before system shutdown.
 * Flow:
 * 1. User selects "Shutdown System" from main menu
 * 2. Confirmation dialog appears with "YES, Shutdown" / "NO, Cancel" options
 * 3. UP/DOWN navigates between options
 * 4. MENU/VAL confirms selection
 * 5. If YES: proceed with shutdown. If NO: return to main menu
 */
class ShutdownConfirmationViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString message READ message CONSTANT)
    Q_PROPERTY(
        int selectedOption READ selectedOption WRITE setSelectedOption NOTIFY selectedOptionChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(
        QString statusMessage READ statusMessage WRITE setStatusMessage NOTIFY statusMessageChanged)

public:
    explicit ShutdownConfirmationViewModel(QObject* parent = nullptr);

    // Options
    enum Option { YesShutdown = 0, NoCancel = 1 };
    Q_ENUM(Option)

    // Property getters
    bool visible() const {
        return m_visible;
    }
    QString title() const {
        return m_title;
    }
    QString message() const {
        return m_message;
    }
    int selectedOption() const {
        return m_selectedOption;
    }
    QColor accentColor() const {
        return m_accentColor;
    }
    QString statusMessage() const {
        return m_statusMessage;
    }

    // Property setters
    void setVisible(bool visible);
    void setSelectedOption(int option);
    void setStatusMessage(const QString& message);

public slots:
    void setAccentColor(const QColor& color);
    void reset();  // Reset to default state when showing

signals:
    void visibleChanged();
    void selectedOptionChanged();
    void accentColorChanged();
    void statusMessageChanged();

private:
    bool m_visible = false;
    QString m_title = "SHUTDOWN CONFIRMATION";
    QString m_message = "Are you sure you want to shutdown the system?";
    int m_selectedOption = NoCancel;              // Default to NO for safety
    QColor m_accentColor = QColor(70, 226, 165);  // Default green
    QString m_statusMessage;
};



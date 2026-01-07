#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
/**
 * @file PresetHomePositionViewModel.h
 * @brief ViewModel for the Preset Home Position overlay UI
 *
 * This ViewModel provides bindable properties for the QML overlay that
 * guides the operator through setting the current gimbal position as
 * the home reference point.
 *
 * @author Claude Code
 * @date December 2025
 */

#include <QObject>
#include <QColor>

class PresetHomePositionViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString instruction READ instruction NOTIFY instructionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double currentAzimuthDeg READ currentAzimuthDeg NOTIFY currentAzimuthDegChanged)
    Q_PROPERTY(
        double currentElevationDeg READ currentElevationDeg NOTIFY currentElevationDegChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)

public:
    explicit PresetHomePositionViewModel(QObject* parent = nullptr);

    // Property getters
    bool visible() const {
        return m_visible;
    }
    QString title() const {
        return m_title;
    }
    QString instruction() const {
        return m_instruction;
    }
    QString status() const {
        return m_status;
    }
    double currentAzimuthDeg() const {
        return m_currentAzimuthDeg;
    }
    double currentElevationDeg() const {
        return m_currentElevationDeg;
    }
    QColor accentColor() const {
        return m_accentColor;
    }

public slots:
    // Property setters
    void setVisible(bool visible);
    void setTitle(const QString& title);
    void setInstruction(const QString& instruction);
    void setStatus(const QString& status);
    void setCurrentAzimuthDeg(double degrees);
    void setCurrentElevationDeg(double degrees);
    void setAccentColor(const QColor& color);

signals:
    void visibleChanged();
    void titleChanged();
    void instructionChanged();
    void statusChanged();
    void currentAzimuthDegChanged();
    void currentElevationDegChanged();
    void accentColorChanged();

private:
    bool m_visible = false;
    QString m_title = "Preset Home Position";
    QString m_instruction;
    QString m_status;
    double m_currentAzimuthDeg = 0.0;
    double m_currentElevationDeg = 0.0;
    QColor m_accentColor = QColor(70, 226, 165);  // Default green accent
};



#ifndef HOMECALIBRATIONVIEWMODEL_H
#define HOMECALIBRATIONVIEWMODEL_H

/**
 * @file HomeCalibrationViewModel.h
 * @brief ViewModel for the Azimuth Home Calibration overlay UI
 *
 * This ViewModel provides bindable properties for the QML overlay that
 * guides the operator through the home calibration procedure to compensate
 * for ABZO encoder drift in the Oriental Motor AZD-KD servo.
 *
 * @author Claude Code
 * @date December 2025
 */

#include <QObject>
#include <QColor>

class HomeCalibrationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString instruction READ instruction NOTIFY instructionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double currentEncoderSteps READ currentEncoderSteps NOTIFY currentEncoderStepsChanged)
    Q_PROPERTY(double currentOffsetSteps READ currentOffsetSteps NOTIFY currentOffsetStepsChanged)
    Q_PROPERTY(double currentAzimuthDeg READ currentAzimuthDeg NOTIFY currentAzimuthDegChanged)
    Q_PROPERTY(bool offsetApplied READ offsetApplied NOTIFY offsetAppliedChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)

public:
    explicit HomeCalibrationViewModel(QObject *parent = nullptr);

    // Property getters
    bool visible() const { return m_visible; }
    QString title() const { return m_title; }
    QString instruction() const { return m_instruction; }
    QString status() const { return m_status; }
    double currentEncoderSteps() const { return m_currentEncoderSteps; }
    double currentOffsetSteps() const { return m_currentOffsetSteps; }
    double currentAzimuthDeg() const { return m_currentAzimuthDeg; }
    bool offsetApplied() const { return m_offsetApplied; }
    QColor accentColor() const { return m_accentColor; }

public slots:
    // Property setters
    void setVisible(bool visible);
    void setTitle(const QString& title);
    void setInstruction(const QString& instruction);
    void setStatus(const QString& status);
    void setCurrentEncoderSteps(double steps);
    void setCurrentOffsetSteps(double steps);
    void setCurrentAzimuthDeg(double degrees);
    void setOffsetApplied(bool applied);
    void setAccentColor(const QColor& color);

signals:
    void visibleChanged();
    void titleChanged();
    void instructionChanged();
    void statusChanged();
    void currentEncoderStepsChanged();
    void currentOffsetStepsChanged();
    void currentAzimuthDegChanged();
    void offsetAppliedChanged();
    void accentColorChanged();

private:
    bool m_visible = false;
    QString m_title = "Azimuth Home Calibration";
    QString m_instruction;
    QString m_status;
    double m_currentEncoderSteps = 0.0;
    double m_currentOffsetSteps = 0.0;
    double m_currentAzimuthDeg = 0.0;
    bool m_offsetApplied = false;
    QColor m_accentColor = QColor(70, 226, 165); // Default green accent
};

#endif // HOMECALIBRATIONVIEWMODEL_H

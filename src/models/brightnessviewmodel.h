#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>
#include <QColor>

class BrightnessViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString instruction READ instruction NOTIFY instructionChanged)
    Q_PROPERTY(bool showParameters READ showParameters NOTIFY showParametersChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY brightnessChanged)
    Q_PROPERTY(QString parameterLabel READ parameterLabel NOTIFY parameterLabelChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)

public:
    explicit BrightnessViewModel(QObject* parent = nullptr);

    bool visible() const {
        return m_visible;
    }
    QString title() const {
        return m_title;
    }
    QString instruction() const {
        return m_instruction;
    }
    bool showParameters() const {
        return m_showParameters;
    }
    int brightness() const {
        return m_brightness;
    }
    QString parameterLabel() const {
        return m_parameterLabel;
    }
    QColor accentColor() const {
        return m_accentColor;
    }

public slots:
    void setVisible(bool visible);
    void setTitle(const QString& title);
    void setInstruction(const QString& instruction);
    void setShowParameters(bool show);
    void setBrightness(int brightness);
    void setParameterLabel(const QString& label);
    void setAccentColor(const QColor& color);

signals:
    void visibleChanged();
    void titleChanged();
    void instructionChanged();
    void showParametersChanged();
    void brightnessChanged();
    void parameterLabelChanged();
    void accentColorChanged();

private:
    bool m_visible;
    QString m_title;
    QString m_instruction;
    bool m_showParameters;
    int m_brightness;
    QString m_parameterLabel;
    QColor m_accentColor = QColor(70, 226, 165);  // Default green
};



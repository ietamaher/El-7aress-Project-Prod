#ifndef ZEROINGVIEWMODEL_H
#define ZEROINGVIEWMODEL_H

#include <QObject>
#include <QColor>

class ZeroingViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString instruction READ instruction NOTIFY instructionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool showOffsets READ showOffsets NOTIFY showOffsetsChanged)
    Q_PROPERTY(float azimuthOffset READ azimuthOffset NOTIFY azimuthOffsetChanged)
    Q_PROPERTY(float elevationOffset READ elevationOffset NOTIFY elevationOffsetChanged)

public:
    explicit ZeroingViewModel(QObject* parent = nullptr);

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
    bool showOffsets() const {
        return m_showOffsets;
    }
    float azimuthOffset() const {
        return m_azimuthOffset;
    }
    float elevationOffset() const {
        return m_elevationOffset;
    }

public slots:
    void setVisible(bool visible);
    void setTitle(const QString& title);
    void setInstruction(const QString& instruction);
    void setStatus(const QString& status);
    void setShowOffsets(bool show);
    void setAzimuthOffset(float offset);
    void setElevationOffset(float offset);
    void setAccentColor(const QColor& color);

signals:
    void visibleChanged();
    void titleChanged();
    void instructionChanged();
    void statusChanged();
    void showOffsetsChanged();
    void azimuthOffsetChanged();
    void elevationOffsetChanged();
    void accentColorChanged();

private:
    bool m_visible;
    QString m_title;
    QString m_instruction;
    QString m_status;
    bool m_showOffsets;
    float m_azimuthOffset;
    float m_elevationOffset;
    QColor m_accentColor = QColor(70, 226, 165);  // Default green
};

#endif  // ZEROINGVIEWMODEL_H

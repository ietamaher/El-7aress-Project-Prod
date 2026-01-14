#ifndef RADARTARGETLISTVIEWMODEL_H
#define RADARTARGETLISTVIEWMODEL_H

#include <QObject>
#include <QStringListModel>
#include <QColor>

/**
 * @brief RadarTargetListViewModel - ViewModel for radar target list QML component
 *
 * Exposes radar target list data to QML:
 * - List of targets (with first row empty for "no selection")
 * - Current selection index
 * - Visibility state
 * - Accent color for styling
 */
class RadarTargetListViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QStringListModel* targetsModel READ targetsModel CONSTANT)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(int targetCount READ targetCount NOTIFY targetCountChanged)

public:
    explicit RadarTargetListViewModel(QObject *parent = nullptr);

    // Property getters
    bool visible() const { return m_visible; }
    QString title() const { return m_title; }
    int currentIndex() const { return m_currentIndex; }
    QStringListModel* targetsModel() { return &m_targetsModel; }
    QColor accentColor() const { return m_accentColor; }
    int targetCount() const { return m_targetsModel.rowCount(); }

public slots:
    // Called by controller
    void setVisible(bool visible);
    void setTitle(const QString& title);
    void setCurrentIndex(int index);
    void setTargetList(const QStringList& targets);
    void setAccentColor(const QColor& color);

signals:
    void visibleChanged();
    void titleChanged();
    void currentIndexChanged();
    void accentColorChanged();
    void targetCountChanged();

private:
    bool m_visible = false;
    QString m_title = "RADAR TARGETS";
    int m_currentIndex = 0;
    QStringListModel m_targetsModel;
    QColor m_accentColor = QColor(70, 226, 165);  // Default green accent
};

#endif // RADARTARGETLISTVIEWMODEL_H

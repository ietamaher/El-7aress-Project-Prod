#ifndef ABOUTVIEWMODEL_H
#define ABOUTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QColor>

/**
 * @brief AboutViewModel - Application information for About dialog
 */
class AboutViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString buildDate READ buildDate CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString credits READ credits CONSTANT)
    Q_PROPERTY(QString copyright READ copyright CONSTANT)
    Q_PROPERTY(QString license READ license CONSTANT)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)


public:
    explicit AboutViewModel(QObject *parent = nullptr);

    QString appName() const { return m_appName; }
    QString appVersion() const { return m_appVersion; }
    QString buildDate() const { return m_buildDate; }
    QString qtVersion() const { return m_qtVersion; }
    QString credits() const { return m_credits; }
    QString copyright() const { return m_copyright; }
    QString license() const { return m_license; }
    QColor accentColor() const { return m_accentColor; }

    bool visible() const { return m_visible; }
    void setVisible(bool visible);

public slots:
    void setAccentColor(const QColor& color);

signals:
    void visibleChanged();
    void accentColorChanged();

private:
    QString m_appName;
    QString m_appVersion;
    QString m_buildDate;
    QString m_qtVersion;
    QString m_credits;
    QString m_copyright;
    QString m_license;
    bool m_visible;

    QColor m_accentColor = QColor(70, 226, 165); // Default green
};

#endif // ABOUTVIEWMODEL_H

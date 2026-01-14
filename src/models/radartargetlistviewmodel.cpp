#include "models/radartargetlistviewmodel.h"
#include <QDebug>

RadarTargetListViewModel::RadarTargetListViewModel(QObject *parent)
    : QObject(parent)
{
}

void RadarTargetListViewModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
        qDebug() << "RadarTargetListViewModel: Visibility changed to" << visible;
    }
}

void RadarTargetListViewModel::setTitle(const QString& title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void RadarTargetListViewModel::setCurrentIndex(int index)
{
    if (m_currentIndex != index) {
        m_currentIndex = index;
        emit currentIndexChanged();
        qDebug() << "RadarTargetListViewModel: Current index changed to" << index;
    }
}

void RadarTargetListViewModel::setTargetList(const QStringList& targets)
{
    m_targetsModel.setStringList(targets);
    emit targetCountChanged();
    qDebug() << "RadarTargetListViewModel: Target list updated with" << targets.size() << "items";
}

void RadarTargetListViewModel::setAccentColor(const QColor& color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}

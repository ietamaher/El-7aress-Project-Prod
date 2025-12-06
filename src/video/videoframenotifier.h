#ifndef VIDEOFRAMENOTIFIER_H
#define VIDEOFRAMENOTIFIER_H

#include <QObject>

class VideoFrameNotifier : public QObject
{
    Q_OBJECT
public:
    explicit VideoFrameNotifier(QObject* parent = nullptr) : QObject(parent) {}
signals:
    void frameUpdated();
};

#endif // VIDEOFRAMENOTIFIER_H

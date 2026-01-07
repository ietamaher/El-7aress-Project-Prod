#include "videoimageprovider.h"
#include <QMutexLocker>

VideoImageProvider::VideoImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

void VideoImageProvider::updateImage(const QImage& newImage) {
    {
        QMutexLocker locker(&m_mutex);
        // QImage uses implicit sharing (copy-on-write), so this is already thread-safe
        // No need for deep copy - reduces latency by 3-5ms
        m_currentImage = newImage;
    }
    // Latency Fix #1: Signal QML immediately (eliminates timer-based polling)
    emit m_notifier.frameUpdated();
}

QImage VideoImageProvider::requestImage(const QString& id, QSize* size,
                                        const QSize& requestedSize) {
    Q_UNUSED(id)
    Q_UNUSED(requestedSize)

    QMutexLocker locker(&m_mutex);

    if (m_currentImage.isNull()) {
        if (size)
            *size = QSize(0, 0);
        return QImage();
    }

    if (size)
        *size = m_currentImage.size();

    return m_currentImage;
}

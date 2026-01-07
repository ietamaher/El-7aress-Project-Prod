#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QQuickImageProvider>
#include <QObject>
#include <QImage>
#include <QMutex>

#include "videoframenotifier.h"
// ============================================================================
// VIDEO FRAME NOTIFIER (Latency Fix #1)
// QQuickImageProvider can't emit signals (no QObject), so we use a separate
// notifier class. QML connects to this instead of using a polling Timer.
// ============================================================================
/*class VideoFrameNotifier : public QObject
{
    Q_OBJECT
public:
    explicit VideoFrameNotifier(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void frameUpdated();
};*/

/**
 * @brief VideoImageProvider - Thread-safe image provider for QML
 *
 * Provides video frames to QML Image components via "image://video/..." URL.
 * Thread-safe for use with CameraVideoStreamDevice running in separate threads.
 */
class VideoImageProvider : public QQuickImageProvider {
public:
    VideoImageProvider();

    /**
     * @brief Update the current image (called from camera threads)
     * @param newImage The new video frame to display
     */
    void updateImage(const QImage& newImage);

    /**
     * @brief QML calls this to get the image
     * @param id Image identifier (usually "camera")
     * @param size Output parameter for image size
     * @param requestedSize Requested size (ignored, we return actual size)
     * @return The current video frame
     */
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    /**
     * @brief Get the notifier object for QML signal connections
     * Latency Fix #1: Eliminates timer-based polling
     */
    VideoFrameNotifier* notifier() {
        return &m_notifier;
    }

private:
    QImage m_currentImage;
    mutable QMutex m_mutex;
    VideoFrameNotifier m_notifier;
};


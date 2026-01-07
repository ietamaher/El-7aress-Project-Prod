#ifndef GSTVIDEOSOURCE_H
#define GSTVIDEOSOURCE_H

#include <QObject>
#include <QImage>
#include <QSize>
#include <QMutex>

// GStreamer includes
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

class VideoImageProvider;  // Forward declaration

class GstVideoSource : public QObject {
    Q_OBJECT

public:
    explicit GstVideoSource(VideoImageProvider* imageProvider, QObject* parent = nullptr);
    ~GstVideoSource();

    Q_INVOKABLE void startPipeline();
    Q_INVOKABLE void stopPipeline();

private:
    GstElement* m_pipeline = nullptr;
    VideoImageProvider* m_imageProvider;
    GstVideoInfo m_videoInfo;
    GstBuffer* m_buffer = nullptr;

    // --- Configuration values (use your existing ones) ---
    QString m_deviceName = "/dev/video0";
    int m_sourceWidth = 1280;
    int m_sourceHeight = 720;
    int m_cropTop = 0;
    int m_cropBottom = 0;
    int m_cropLeft = 0;
    int m_cropRight = 0;

    // GStreamer callbacks
    static GstFlowReturn onNewSample(GstElement* sink, GstVideoSource* self);

signals:
    // Signal to notify QML that the video frame has been updated.
    void frameUpdated();
};

#endif  // GSTVIDEOSOURCE_H

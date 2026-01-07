#include "gstvideosource.h"
#include "videoimageprovider.h"

// GStreamer error handling helper macro
#define GST_CHECK(condition)                                    \
    if (!(condition)) {                                         \
        qWarning("GStreamer condition failed: %s", #condition); \
        return;                                                 \
    }

GstVideoSource::GstVideoSource(VideoImageProvider* imageProvider, QObject* parent)
    : QObject(parent), m_imageProvider(imageProvider) {
    // Initialize GStreamer if not already done by main()
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }
}

GstVideoSource::~GstVideoSource() {
    stopPipeline();
}

void GstVideoSource::startPipeline() {
    if (m_pipeline) {
        qWarning("Pipeline already running.");
        return;
    }

    // --- Your GStreamer pipeline string ---
    // Note: Added `videoconvert` and `video/x-raw,format=RGB` to ensure QImage compatibility.
    // The previous pipeline would leave the format up to negotiation, which might not be RGB.
    // We explicitly convert to RGB for easy display in QML.
    /*QString pipelineStr = QString(
                              "v4l2src device=%1 do-timestamp=true ! "
                              "video/x-raw,format=YUY2,width=%2,height=%3,framerate=30/1 ! "
                              "videocrop top=%4 left=%6 bottom=%5 right=%7 ! "
                              "videoscale ! "
                              "video/x-raw,width=1024,height=768 ! "
                              "videoconvert ! " // Add a converter to ensure RGB output
                              "video/x-raw,format=RGB ! " // Explicitly request RGB format for QImage
                              "appsink name=mysink emit-signals=true max-buffers=2 drop=true sync=false")
                              .arg(m_deviceName)
                              .arg(m_sourceWidth)
                              .arg(m_sourceHeight)
                              .arg(m_cropTop)
                              .arg(m_cropBottom)
                              .arg(m_cropLeft)
                              .arg(m_cropRight);*/

    QString pipelineStr =
        QString("v4l2src device=%1 do-timestamp=true ! "
                "image/jpeg,width=%2,height=%3,framerate=30/1 ! jpegdec ! video/x-raw ! "
                "aspectratiocrop aspect-ratio=4/3 ! "
                "videoscale  ! "
                "video/x-raw,width=1024,height=768 ! "
                "videoconvert ! video/x-raw,format=RGB ! "  // Explicit conversion
                "queue max-size-buffers=2 leaky=downstream ! "
                "appsink name=mysink emit-signals=true max-buffers=2 drop=true sync=false")
            .arg(m_deviceName)
            .arg(m_sourceWidth)
            .arg(m_sourceHeight);

    qDebug() << "Launching GStreamer pipeline:\n" << pipelineStr;

    // Create the pipeline from the string
    GError* error = nullptr;
    m_pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);
    if (!m_pipeline || error) {
        qCritical() << "Failed to parse pipeline:" << (error ? error->message : "Unknown error");
        g_clear_error(&error);
        return;
    }

    // Configure the appsink to send frames to our C++ handler
    GstElement* appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
    if (appsink) {
        g_signal_connect(appsink, "new-sample", G_CALLBACK(onNewSample), this);
        gst_object_unref(appsink);  // Decrement reference count, ownership held by pipeline
    } else {
        qCritical() << "Failed to find 'mysink' element.";
        return;
    }

    // Start the pipeline playing
    GST_CHECK(gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_ASYNC);
    qDebug() << "Pipeline started successfully.";
}

void GstVideoSource::stopPipeline() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        qDebug() << "Pipeline stopped.";
    }
}

GstFlowReturn GstVideoSource::onNewSample(GstElement* sink, GstVideoSource* self) {
    // 1. Get GstSample from appsink
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
        return GST_FLOW_OK;
    }

    // 2. Extract buffer and video info
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstVideoInfo videoInfo;
    gst_video_info_from_caps(&videoInfo, caps);

    // 3. Map GstBuffer to memory
    GstMapInfo mapInfo;
    gst_buffer_map(buffer, &mapInfo, GST_MAP_READ);

    if (mapInfo.data) {
        // 4. Create QImage from the raw buffer data
        // We assume the pipeline configured in startPipeline() delivers RGB format.
        // If you change the format, you must change QImage format accordingly.
        QImage frameImage(mapInfo.data, videoInfo.width, videoInfo.height, QImage::Format_RGB888);

        // 5. Update the QML image provider
        self->m_imageProvider->updateImage(frameImage);

        // 6. Notify QML to update UI
        emit self->frameUpdated();
    }

    gst_buffer_unmap(buffer, &mapInfo);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

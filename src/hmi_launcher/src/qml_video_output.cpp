/**
 * @file qml_video_output.cpp
 * @brief GStreamer-based video output implementation for OpenAuto
 *
 * Copyright (C) 2024 Speeduino UI Project
 * License: GPL-3.0
 */

#include "hmi/qml_video_output.hpp"
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QImage>

namespace speeduino {

// Priority list of H.264 decoders to try (in order of preference)
// NOTE: avdec_h264 (FFmpeg) is first because v4l2h264dec has issues with
// async state changes when receiving live H.264 stream from appsrc.
// v4l2h264dec gets stuck in PAUSED->PLAYING transition.
static const char* DECODER_PRIORITY[] = {
    "avdec_h264",    // FFmpeg software decoder (reliable with live streams)
    "v4l2h264dec",   // Raspberry Pi hardware decoder (can hang on state change)
    "nvh264dec",     // NVIDIA hardware decoder
    "vaapih264dec",  // VAAPI hardware decoder
    nullptr
};

QMLVideoOutput::QMLVideoOutput(openauto::configuration::IConfiguration::Pointer configuration,
                               QObject* parent)
    : QObject(parent)
    , openauto::projection::VideoOutput(std::move(configuration))
{
    qInfo() << "[QMLVideoOutput] Creating GStreamer-based video output";

    // Get video dimensions from configuration
    switch (this->configuration_->getVideoResolution()) {
    case aasdk::proto::enums::VideoResolution_Enum__1080p:
        m_width = 1920;
        m_height = 1080;
        break;
    case aasdk::proto::enums::VideoResolution_Enum__720p:
        m_width = 1280;
        m_height = 720;
        break;
    case aasdk::proto::enums::VideoResolution_Enum__480p:
    default:
        m_width = 800;
        m_height = 480;
        break;
    }

    qInfo() << "[QMLVideoOutput] Video resolution:" << m_width << "x" << m_height;

    // Move to main thread for Qt operations
    QCoreApplication* app = QCoreApplication::instance();
    if (app && QThread::currentThread() != app->thread()) {
        this->moveToThread(app->thread());
    }

    // Create frame processing timer
    m_frameTimer = new QTimer(this);
    m_frameTimer->setInterval(16);  // ~60 FPS polling
    connect(m_frameTimer, &QTimer::timeout, this, &QMLVideoOutput::processFrames);
}

QMLVideoOutput::~QMLVideoOutput()
{
    qInfo() << "[QMLVideoOutput] Destroying";

    m_playing.store(false);
    m_opened.store(false);

    if (m_frameTimer) {
        m_frameTimer->stop();
    }

    cleanupGStreamer();
}

QString QMLVideoOutput::findBestDecoder()
{
    for (int i = 0; DECODER_PRIORITY[i] != nullptr; ++i) {
        GstElementFactory* factory = gst_element_factory_find(DECODER_PRIORITY[i]);
        if (factory != nullptr) {
            gst_object_unref(factory);
            qInfo() << "[QMLVideoOutput] Found decoder:" << DECODER_PRIORITY[i];
            return QString(DECODER_PRIORITY[i]);
        }
    }
    qWarning() << "[QMLVideoOutput] No H.264 decoder found!";
    return QString();
}

bool QMLVideoOutput::initGStreamer()
{
    if (m_gstInitialized.load()) {
        return true;
    }

    qInfo() << "[QMLVideoOutput] Initializing GStreamer pipeline";

    // Initialize GStreamer if not already done
    if (!gst_is_initialized()) {
        GError* error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &error)) {
            qCritical() << "[QMLVideoOutput] GStreamer init failed:"
                        << (error ? error->message : "unknown error");
            if (error) g_error_free(error);
            return false;
        }
    }

    // Find best available decoder
    QString decoder = findBestDecoder();
    if (decoder.isEmpty()) {
        qCritical() << "[QMLVideoOutput] No H.264 decoder available";
        return false;
    }

    // Build pipeline string
    // Pipeline: appsrc -> h264parse -> decoder -> videoconvert -> appsink
    QString pipelineStr = QString(
        "appsrc name=src is-live=true format=time do-timestamp=true max-latency=100000000 ! "
        "queue max-size-buffers=0 max-size-time=0 max-size-bytes=0 ! "
        "h264parse ! "
        "%1 ! "
        "videoconvert ! "
        "video/x-raw,format=RGB ! "
        "appsink name=sink emit-signals=true sync=false"
    ).arg(decoder);

    qInfo() << "[QMLVideoOutput] Pipeline:" << pipelineStr;

    // Create pipeline
    GError* error = nullptr;
    m_pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);
    if (!m_pipeline) {
        qCritical() << "[QMLVideoOutput] Failed to create pipeline:"
                    << (error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return false;
    }
    if (error) {
        qWarning() << "[QMLVideoOutput] Pipeline warning:" << error->message;
        g_error_free(error);
    }

    // Get appsrc element
    GstElement* srcElement = gst_bin_get_by_name(GST_BIN(m_pipeline), "src");
    if (!srcElement) {
        qCritical() << "[QMLVideoOutput] Failed to get appsrc element";
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return false;
    }
    m_appSrc = GST_APP_SRC(srcElement);

    // Configure appsrc for H.264 stream
    GstCaps* caps = gst_caps_new_simple("video/x-h264",
                                        "stream-format", G_TYPE_STRING, "byte-stream",
                                        "alignment", G_TYPE_STRING, "au",
                                        "width", G_TYPE_INT, m_width,
                                        "height", G_TYPE_INT, m_height,
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        nullptr);
    gst_app_src_set_caps(m_appSrc, caps);
    gst_caps_unref(caps);

    gst_app_src_set_stream_type(m_appSrc, GST_APP_STREAM_TYPE_STREAM);
    gst_app_src_set_max_bytes(m_appSrc, 4 * 1024 * 1024);  // 4MB buffer

    // Don't block when pushing buffers - important for live streams
    g_object_set(G_OBJECT(m_appSrc), "block", FALSE, nullptr);

    // Get appsink element
    GstElement* sinkElement = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!sinkElement) {
        qCritical() << "[QMLVideoOutput] Failed to get appsink element";
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return false;
    }
    m_appSink = GST_APP_SINK(sinkElement);

    // Configure appsink
    gst_app_sink_set_emit_signals(m_appSink, TRUE);
    gst_app_sink_set_drop(m_appSink, TRUE);  // Drop frames if we can't keep up
    gst_app_sink_set_max_buffers(m_appSink, 2);  // Keep only recent frames

    // Set up bus for error handling
    m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    gst_bus_add_watch(m_bus, &QMLVideoOutput::onBusMessage, this);

    m_gstInitialized.store(true);
    qInfo() << "[QMLVideoOutput] GStreamer pipeline initialized successfully";
    return true;
}

void QMLVideoOutput::cleanupGStreamer()
{
    qInfo() << "[QMLVideoOutput] Cleaning up GStreamer";

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }

    if (m_bus) {
        gst_bus_remove_watch(m_bus);
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    // Note: m_appSrc and m_appSink are owned by the pipeline
    m_appSrc = nullptr;
    m_appSink = nullptr;

    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    m_gstInitialized.store(false);
}

gboolean QMLVideoOutput::onBusMessage(GstBus* /*bus*/, GstMessage* message, gpointer userData)
{
    QMLVideoOutput* self = static_cast<QMLVideoOutput*>(userData);

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(message, &err, &debug);
        qCritical() << "[QMLVideoOutput] GStreamer error:" << err->message;
        qDebug() << "[QMLVideoOutput] Debug info:" << debug;
        g_error_free(err);
        g_free(debug);

        QMetaObject::invokeMethod(self, [self]() {
            emit self->errorOccurred("GStreamer pipeline error");
        }, Qt::QueuedConnection);
        break;
    }
    case GST_MESSAGE_WARNING: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_warning(message, &err, &debug);
        qWarning() << "[QMLVideoOutput] GStreamer warning:" << err->message;
        g_error_free(err);
        g_free(debug);
        break;
    }
    case GST_MESSAGE_EOS:
        qInfo() << "[QMLVideoOutput] End of stream";
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->m_pipeline)) {
            GstState oldState, newState, pending;
            gst_message_parse_state_changed(message, &oldState, &newState, &pending);
            qDebug() << "[QMLVideoOutput] Pipeline state:"
                     << gst_element_state_get_name(oldState) << "->"
                     << gst_element_state_get_name(newState);
        }
        break;
    default:
        break;
    }

    return TRUE;
}

// ═══════════════════════════════════════════════════════════════════════════
// IVideoOutput interface implementation
// ═══════════════════════════════════════════════════════════════════════════

bool QMLVideoOutput::open()
{
    QMutexLocker locker(&m_mutex);

    if (m_opened.load()) {
        qDebug() << "[QMLVideoOutput] Already opened";
        return true;
    }

    qInfo() << "[QMLVideoOutput] Opening video output";

    if (!initGStreamer()) {
        return false;
    }

    // Start pipeline in READY state (will go to PLAYING in init())
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qCritical() << "[QMLVideoOutput] Failed to set pipeline to READY";
        return false;
    }

    m_opened.store(true);
    qInfo() << "[QMLVideoOutput] Video output opened successfully";
    return true;
}

bool QMLVideoOutput::init()
{
    qInfo() << "[QMLVideoOutput] Initializing playback";

    // Start playback on main thread
    QMetaObject::invokeMethod(this, &QMLVideoOutput::doStartPlayback,
                              Qt::QueuedConnection);
    return true;
}

void QMLVideoOutput::write(uint64_t timestamp, const aasdk::common::DataConstBuffer& buffer)
{
    if (!m_opened.load() || !m_appSrc) {
        return;
    }

    // Debug: log every 30th frame to avoid spam
    static int writeCount = 0;
    if (++writeCount % 30 == 1) {
        qDebug() << "[QMLVideoOutput] write() called, size:" << buffer.size << "bytes, frame#" << writeCount;
    }

    // Create GStreamer buffer
    GstBuffer* gstBuffer = gst_buffer_new_and_alloc(buffer.size);
    if (!gstBuffer) {
        qWarning() << "[QMLVideoOutput] Failed to allocate GStreamer buffer";
        return;
    }

    // Copy data to GStreamer buffer
    GstMapInfo map;
    if (gst_buffer_map(gstBuffer, &map, GST_MAP_WRITE)) {
        memcpy(map.data, buffer.cdata, buffer.size);
        gst_buffer_unmap(gstBuffer, &map);
    } else {
        gst_buffer_unref(gstBuffer);
        qWarning() << "[QMLVideoOutput] Failed to map GStreamer buffer";
        return;
    }

    // Set timestamp (convert from microseconds to nanoseconds)
    GST_BUFFER_PTS(gstBuffer) = timestamp * 1000;
    GST_BUFFER_DTS(gstBuffer) = GST_CLOCK_TIME_NONE;

    // Push buffer to appsrc
    GstFlowReturn ret = gst_app_src_push_buffer(m_appSrc, gstBuffer);
    if (ret != GST_FLOW_OK) {
        // Buffer is consumed by push_buffer even on error
        qDebug() << "[QMLVideoOutput] Push buffer returned:" << ret;
    }
}

void QMLVideoOutput::stop()
{
    qInfo() << "[QMLVideoOutput] Stopping playback";

    // Use BlockingQueuedConnection to ensure stop completes
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        doStopPlayback();
        return;
    }

    const bool onMainThread = (QThread::currentThread() == app->thread());
    if (onMainThread) {
        doStopPlayback();
    } else {
        QMetaObject::invokeMethod(this, &QMLVideoOutput::doStopPlayback,
                                  Qt::BlockingQueuedConnection);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// QML interface
// ═══════════════════════════════════════════════════════════════════════════

void QMLVideoOutput::setVideoSink(QVideoSink* sink)
{
    QMutexLocker locker(&m_mutex);

    if (m_videoSink == sink) {
        return;
    }

    qInfo() << "[QMLVideoOutput] Setting video sink:" << sink;
    m_videoSink = sink;
    emit videoSinkChanged();
}

// ═══════════════════════════════════════════════════════════════════════════
// Private slots
// ═══════════════════════════════════════════════════════════════════════════

void QMLVideoOutput::doStartPlayback()
{
    QMutexLocker locker(&m_mutex);

    if (!m_pipeline) {
        qWarning() << "[QMLVideoOutput] Cannot start - no pipeline";
        return;
    }

    // Note: We now start even without video sink - it can be set later
    if (!m_videoSink) {
        qWarning() << "[QMLVideoOutput] Starting without video sink - waiting for QML to connect";
    }

    qInfo() << "[QMLVideoOutput] Starting GStreamer pipeline";

    // Start the pipeline (don't wait for state change - it may need data first)
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qCritical() << "[QMLVideoOutput] Failed to start pipeline";
        emit errorOccurred("Failed to start video pipeline");
        return;
    }
    // GST_STATE_CHANGE_ASYNC is expected - pipeline will complete transition after receiving data
    if (ret == GST_STATE_CHANGE_ASYNC) {
        qDebug() << "[QMLVideoOutput] Pipeline state change async (will complete after receiving data)";
    }

    // Start frame processing timer
    m_frameTimer->start();

    m_playing.store(true);
    emit playingChanged();
    emit playbackStarted();

    qInfo() << "[QMLVideoOutput] Playback started, videoSink:" << m_videoSink;
}

void QMLVideoOutput::doStopPlayback()
{
    QMutexLocker locker(&m_mutex);

    if (m_frameTimer) {
        m_frameTimer->stop();
    }

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_READY);
    }

    m_playing.store(false);
    m_opened.store(false);

    emit playingChanged();
    emit playbackStopped();

    qInfo() << "[QMLVideoOutput] Playback stopped";
}

void QMLVideoOutput::processFrames()
{
    // Debug: log why we're not processing
    static int skipCount = 0;
    if (!m_playing.load()) {
        if (++skipCount % 100 == 1) {
            qDebug() << "[QMLVideoOutput] processFrames() skipped - not playing";
        }
        return;
    }
    if (!m_appSink) {
        if (++skipCount % 100 == 1) {
            qDebug() << "[QMLVideoOutput] processFrames() skipped - no appSink";
        }
        return;
    }
    if (!m_videoSink) {
        if (++skipCount % 100 == 1) {
            qDebug() << "[QMLVideoOutput] processFrames() skipped - no videoSink";
        }
        return;
    }
    skipCount = 0;  // Reset when we can actually process

    // Try to pull a sample (non-blocking)
    GstSample* sample = gst_app_sink_try_pull_sample(m_appSink, 0);
    if (sample) {
        static int frameCount = 0;
        if (++frameCount % 30 == 1) {
            qDebug() << "[QMLVideoOutput] processFrames() got sample, frame#" << frameCount;
        }
        handleDecodedFrame(sample);
        gst_sample_unref(sample);
    } else {
        // Debug: check if there's data pending in appsink
        static int noSampleCount = 0;
        if (++noSampleCount % 300 == 1) {  // Every 5 seconds approx
            qDebug() << "[QMLVideoOutput] processFrames() no sample available yet, check#" << noSampleCount;
            // Check pipeline state
            if (m_pipeline) {
                GstState state, pending;
                gst_element_get_state(m_pipeline, &state, &pending, 0);
                qDebug() << "[QMLVideoOutput] Pipeline state:" << gst_element_state_get_name(state)
                         << "pending:" << gst_element_state_get_name(pending);
            }
        }
    }
}

void QMLVideoOutput::handleDecodedFrame(GstSample* sample)
{
    if (!sample || !m_videoSink) {
        return;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);

    if (!buffer || !caps) {
        return;
    }

    // Get video info from caps
    GstVideoInfo videoInfo;
    if (!gst_video_info_from_caps(&videoInfo, caps)) {
        qWarning() << "[QMLVideoOutput] Failed to get video info from caps";
        return;
    }

    int width = GST_VIDEO_INFO_WIDTH(&videoInfo);
    int height = GST_VIDEO_INFO_HEIGHT(&videoInfo);

    // Map buffer for reading
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        qWarning() << "[QMLVideoOutput] Failed to map buffer";
        return;
    }

    // Create QImage from the RGB data
    // Note: We need to copy the data because QImage doesn't take ownership
    QImage image(map.data, width, height, GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0),
                 QImage::Format_RGB888);

    // Create a deep copy so we can unmap the GStreamer buffer
    QImage frameCopy = image.copy();

    gst_buffer_unmap(buffer, &map);

    // Create QVideoFrame from the image
    QVideoFrame frame(frameCopy);

    // Debug: log frame delivery
    static int deliveredCount = 0;
    if (++deliveredCount % 30 == 1) {
        qDebug() << "[QMLVideoOutput] Delivering frame to sink, size:" << width << "x" << height << "frame#" << deliveredCount;
    }

    // Send frame to video sink
    m_videoSink->setVideoFrame(frame);
}

} // namespace speeduino

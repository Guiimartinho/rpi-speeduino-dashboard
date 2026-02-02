/**
 * @file camera_controller.cpp
 * @brief GStreamer-based reverse camera controller implementation
 *
 * Copyright (C) 2024 Speeduino UI Project
 * License: GPL-3.0
 */

#include "hmi/camera_controller.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QThread>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace speeduino {

CameraController::CameraController(QObject* parent) : QObject(parent) {
    qInfo() << "[CameraController] Creating GStreamer-based camera controller";

    // Frame processing timer (~30 FPS polling)
    m_frameTimer = new QTimer(this);
    m_frameTimer->setInterval(33);
    connect(m_frameTimer, &QTimer::timeout, this, &CameraController::processFrames);

    // Device check timer (check every 2 seconds when not active)
    m_deviceCheckTimer = new QTimer(this);
    m_deviceCheckTimer->setInterval(2000);
    connect(m_deviceCheckTimer, &QTimer::timeout, this, &CameraController::onDeviceCheckTimer);

    // Frame timeout timer (detects signal loss after 1 second)
    m_frameTimeoutTimer = new QTimer(this);
    m_frameTimeoutTimer->setInterval(1000);
    connect(m_frameTimeoutTimer, &QTimer::timeout, this, &CameraController::onFrameTimeout);

    // Check for test mode environment variable
    if (qEnvironmentVariableIsSet("HMI_CAMERA_TEST_MODE")) {
        m_testMode = qEnvironmentVariable("HMI_CAMERA_TEST_MODE") == "1";
        if (m_testMode) {
            qInfo() << "[CameraController] Test mode enabled via environment variable";
        }
    }

    // Initial device check after short delay (skip if test mode)
    if (!m_testMode) {
        QTimer::singleShot(500, this, &CameraController::checkAvailability);
    } else {
        // In test mode, camera is always "available"
        setAvailable(true);
        setStatus("Test mode - simulated camera");
    }
}

CameraController::~CameraController() {
    qInfo() << "[CameraController] Destroying";

    m_active.store(false);

    if (m_frameTimer) {
        m_frameTimer->stop();
    }
    if (m_deviceCheckTimer) {
        m_deviceCheckTimer->stop();
    }
    if (m_frameTimeoutTimer) {
        m_frameTimeoutTimer->stop();
    }

    cleanupGStreamer();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void CameraController::setDevice(const QString& device) {
    QMutexLocker locker(&m_mutex);

    if (m_device != device) {
        m_device = device;
        emit deviceChanged();

        // Re-check availability when device changes
        QTimer::singleShot(100, this, &CameraController::checkAvailability);
    }
}

void CameraController::setResolution(int width, int height) {
    QMutexLocker locker(&m_mutex);
    m_width  = width;
    m_height = height;
}

void CameraController::setFramerate(int fps) {
    QMutexLocker locker(&m_mutex);
    m_fps = fps;
}

void CameraController::setVideoStandard(const QString& standard) {
    QMutexLocker locker(&m_mutex);
    m_videoStandard = standard.toUpper();

    // Set default resolution/fps for standard
    if (m_videoStandard == "PAL") {
        m_width  = 720;
        m_height = 576;
        m_fps    = 25;
    } else if (m_videoStandard == "NTSC") {
        m_width  = 720;
        m_height = 480;
        m_fps    = 30;
    }
}

void CameraController::setCompositeInput(int input) {
    QMutexLocker locker(&m_mutex);
    m_compositeInput = input;
}

QVideoSink* CameraController::videoSink() const {
    QMutexLocker locker(&m_mutex);
    return m_videoSink;
}

void CameraController::setVideoSink(QVideoSink* sink) {
    QMutexLocker locker(&m_mutex);

    if (m_videoSink == sink) {
        return;
    }

    qInfo() << "[CameraController] Setting video sink:" << sink;
    m_videoSink = sink;
    emit videoSinkChanged();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test/Simulation Mode
// ═══════════════════════════════════════════════════════════════════════════════

void CameraController::setTestMode(bool enabled) {
    if (m_testMode == enabled) {
        return;
    }

    qInfo() << "[CameraController] Test mode:" << (enabled ? "ENABLED" : "DISABLED");
    m_testMode = enabled;
    emit testModeChanged();

    if (enabled) {
        // In test mode, camera is always "available"
        setAvailable(true);
        setStatus("Test mode - simulated camera");
    } else {
        // Re-check real camera availability
        checkAvailability();
    }
}

void CameraController::setTestPattern(const QString& pattern) {
    QString lowerPattern = pattern.toLower();

    // Validate pattern
    QStringList valid = availableTestPatterns();
    if (!valid.contains(lowerPattern)) {
        qWarning() << "[CameraController] Invalid test pattern:" << pattern
                   << "- using 'ball' instead";
        lowerPattern = "ball";
    }

    if (m_testPattern != lowerPattern) {
        m_testPattern = lowerPattern;
        emit testPatternChanged();
        qInfo() << "[CameraController] Test pattern set to:" << m_testPattern;
    }
}

QStringList CameraController::availableTestPatterns() const {
    return QStringList{
        "ball",        // Moving ball (best for latency testing)
        "smpte",       // SMPTE color bars (alignment testing)
        "snow",        // TV snow (simulates no signal)
        "checkers",    // Checkerboard (resolution testing)
        "gradient",    // Color gradient (banding testing)
        "circular",    // Circular pattern (aspect ratio testing)
        "black",       // Solid black
        "white",       // Solid white
        "red",         // Solid red
        "green",       // Solid green
        "blue"         // Solid blue
    };
}

int CameraController::getTestPatternId(const QString& pattern) const {
    // GStreamer videotestsrc pattern IDs
    static const QHash<QString, int> patterns = {
        {"smpte", 0},
        {"snow", 1},
        {"black", 2},
        {"white", 3},
        {"red", 4},
        {"green", 5},
        {"blue", 6},
        {"checkers", 7},  // checkers-1
        {"circular", 11},
        {"gradient", 23},
        {"ball", 18}
    };

    return patterns.value(pattern.toLower(), 18);  // Default to ball
}

QString CameraController::buildTestPipelineString() const {
    int patternId = getTestPatternId(m_testPattern);

    // Test source pipeline with is-live=true for proper timing
    QString pipeline = QString(
        "videotestsrc pattern=%1 is-live=true name=source ! "
        "video/x-raw,width=%2,height=%3,framerate=%4/1 ! "
        "queue max-size-buffers=2 leaky=downstream ! "
        "videoconvert n-threads=2 ! "
        "video/x-raw,format=RGB ! "
        "appsink name=sink emit-signals=true sync=false drop=true max-buffers=2"
    ).arg(patternId).arg(m_width).arg(m_height).arg(m_fps);

    return pipeline;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Device availability
// ═══════════════════════════════════════════════════════════════════════════════

bool CameraController::checkAvailability() {
    // Check if device file exists
    if (!QFile::exists(m_device)) {
        setStatus("Camera not connected");
        setAvailable(false);

        // Start monitoring for device connection
        if (!m_deviceCheckTimer->isActive()) {
            m_deviceCheckTimer->start();
        }
        return false;
    }

    // Try to open the device
    int fd = open(m_device.toUtf8().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        setStatus("Cannot access camera (permission denied?)");
        setAvailable(false);

        if (!m_deviceCheckTimer->isActive()) {
            m_deviceCheckTimer->start();
        }
        return false;
    }

    // Query V4L2 capabilities
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        close(fd);
        setStatus("Invalid video device");
        setAvailable(false);
        return false;
    }

    close(fd);

    // Check for video capture capability
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        setStatus("Device cannot capture video");
        setAvailable(false);
        return false;
    }

    // Camera is available
    setStatus("Camera ready");
    setAvailable(true);
    m_retryCount = 0;

    // Stop device check timer
    if (m_deviceCheckTimer->isActive()) {
        m_deviceCheckTimer->stop();
    }

    qInfo() << "[CameraController] Camera available:" << m_device
            << "Driver:" << QString::fromUtf8((const char*)cap.driver)
            << "Card:" << QString::fromUtf8((const char*)cap.card);

    return true;
}

void CameraController::onDeviceCheckTimer() {
    // Only check if not active and not available
    if (!m_active.load() && !m_available.load()) {
        checkAvailability();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// GStreamer pipeline
// ═══════════════════════════════════════════════════════════════════════════════

QString CameraController::buildPipelineString() const {
    // Use test pipeline if test mode is enabled
    if (m_testMode) {
        qInfo() << "[CameraController] Building TEST pipeline with pattern:" << m_testPattern;
        return buildTestPipelineString();
    }

    // Low-latency pipeline for USB capture cards / cameras
    // Uses v4l2src with minimal buffering for real-time display
    qInfo() << "[CameraController] Building V4L2 pipeline for device:" << m_device;

    QString pipeline = QString(
        "v4l2src device=%1 do-timestamp=true ! "
        "queue max-size-buffers=2 leaky=downstream ! "
        "videoconvert n-threads=2 ! "
        "video/x-raw,format=RGB ! "
        "appsink name=sink emit-signals=true sync=false drop=true max-buffers=2"
    ).arg(m_device);

    return pipeline;
}

bool CameraController::initGStreamer() {
    if (m_gstInitialized.load()) {
        return true;
    }

    qInfo() << "[CameraController] Initializing GStreamer pipeline";

    // Initialize GStreamer if not already done
    if (!gst_is_initialized()) {
        GError* error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &error)) {
            qCritical() << "[CameraController] GStreamer init failed:"
                        << (error ? error->message : "unknown error");
            if (error) {
                g_error_free(error);
            }
            return false;
        }
    }

    // Build pipeline
    QString pipelineStr = buildPipelineString();
    qInfo() << "[CameraController] Pipeline:" << pipelineStr;

    GError* error = nullptr;
    m_pipeline    = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (!m_pipeline) {
        QString errorMsg = error ? QString::fromUtf8(error->message) : "Unknown error";
        qCritical() << "[CameraController] Failed to create pipeline:" << errorMsg;
        if (error) {
            g_error_free(error);
        }
        setError("Failed to create camera pipeline: " + errorMsg);
        return false;
    }
    if (error) {
        qWarning() << "[CameraController] Pipeline warning:" << error->message;
        g_error_free(error);
    }

    // Get appsink element
    GstElement* sinkElement = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!sinkElement) {
        qCritical() << "[CameraController] Failed to get appsink element";
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return false;
    }
    m_appSink = GST_APP_SINK(sinkElement);

    // Configure appsink
    gst_app_sink_set_emit_signals(m_appSink, TRUE);
    gst_app_sink_set_drop(m_appSink, TRUE);      // Drop frames if we can't keep up
    gst_app_sink_set_max_buffers(m_appSink, 2);  // Keep only recent frames

    // Set up bus for error handling
    m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    gst_bus_add_watch(m_bus, &CameraController::onBusMessage, this);

    m_gstInitialized.store(true);
    qInfo() << "[CameraController] GStreamer pipeline initialized successfully";
    return true;
}

void CameraController::cleanupGStreamer() {
    qInfo() << "[CameraController] Cleaning up GStreamer";

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }

    if (m_bus) {
        gst_bus_remove_watch(m_bus);
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    // Note: m_appSink is owned by the pipeline
    m_appSink = nullptr;

    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    m_gstInitialized.store(false);
}

gboolean CameraController::onBusMessage(GstBus* /*bus*/, GstMessage* message, gpointer userData) {
    CameraController* self = static_cast<CameraController*>(userData);

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* err  = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &err, &debug);

            QString errorMsg = QString::fromUtf8(err->message);
            qCritical() << "[CameraController] GStreamer error:" << errorMsg;
            qDebug() << "[CameraController] Debug info:" << debug;

            g_error_free(err);
            g_free(debug);

            // Handle error on main thread
            QMetaObject::invokeMethod(
                self,
                [self, errorMsg]() {
                    self->setError(errorMsg);
                    self->setActive(false);

                    // Try to recover
                    self->cleanupGStreamer();

                    if (self->m_retryCount < MAX_RETRIES) {
                        self->m_retryCount++;
                        int delay = 1000 * self->m_retryCount;
                        qInfo() << "[CameraController] Retry" << self->m_retryCount << "in" << delay << "ms";
                        QTimer::singleShot(delay, self, [self]() {
                            if (self->checkAvailability()) {
                                self->start();
                            }
                        });
                    } else {
                        self->setStatus("Camera error - too many retries");
                        emit self->cameraError("Camera failed after multiple retries");
                    }
                },
                Qt::QueuedConnection);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err  = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_warning(message, &err, &debug);
            qWarning() << "[CameraController] GStreamer warning:" << err->message;
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            qInfo() << "[CameraController] End of stream";
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    self->setActive(false);
                    self->setStatus("Camera stream ended");
                },
                Qt::QueuedConnection);
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->m_pipeline)) {
                GstState oldState, newState, pending;
                gst_message_parse_state_changed(message, &oldState, &newState, &pending);
                qDebug() << "[CameraController] Pipeline state:"
                         << gst_element_state_get_name(oldState) << "->"
                         << gst_element_state_get_name(newState);

                if (newState == GST_STATE_PLAYING) {
                    QMetaObject::invokeMethod(
                        self,
                        [self]() {
                            self->setActive(true);
                            self->setStatus("Camera active");
                            self->m_retryCount = 0;
                            emit self->cameraReady();
                        },
                        Qt::QueuedConnection);
                }
            }
            break;
        default:
            break;
    }

    return TRUE;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Control
// ═══════════════════════════════════════════════════════════════════════════════

bool CameraController::start() {
    QMutexLocker locker(&m_mutex);

    if (m_active.load()) {
        qDebug() << "[CameraController] Already active";
        return true;
    }

    clearError();

    // Check availability first
    if (!m_available.load()) {
        locker.unlock();
        if (!checkAvailability()) {
            setError("Camera not available");
            return false;
        }
        locker.relock();
    }

    // Initialize GStreamer pipeline
    if (!initGStreamer()) {
        return false;
    }

    if (!m_videoSink) {
        qWarning() << "[CameraController] Starting without video sink - waiting for QML to connect";
    }

    qInfo() << "[CameraController] Starting pipeline";

    // Start the pipeline
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qCritical() << "[CameraController] Failed to start pipeline";
        setError("Failed to start camera");
        cleanupGStreamer();
        return false;
    }

    // Start frame processing timer
    m_frameTimer->start();

    // Start frame timeout timer (detects signal loss)
    m_lastFrameTime.store(QDateTime::currentMSecsSinceEpoch());
    m_frameTimeoutTimer->start();

    setStatus("Starting camera...");
    qInfo() << "[CameraController] Pipeline starting, videoSink:" << m_videoSink
            << ", testMode:" << m_testMode;

    return true;
}

void CameraController::stop() {
    QMutexLocker locker(&m_mutex);

    if (!m_active.load() && !m_gstInitialized.load()) {
        return;
    }

    qInfo() << "[CameraController] Stopping";

    if (m_frameTimer) {
        m_frameTimer->stop();
    }

    if (m_frameTimeoutTimer) {
        m_frameTimeoutTimer->stop();
    }

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }

    setActive(false);
    setStatus("Camera stopped");

    qInfo() << "[CameraController] Stopped";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Frame processing
// ═══════════════════════════════════════════════════════════════════════════════

void CameraController::processFrames() {
    if (!m_active.load() || !m_appSink) {
        return;
    }

    // Check if video sink is available (thread-safe read)
    QVideoSink* sink = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        sink = m_videoSink;
    }

    if (!sink) {
        return;
    }

    // Try to pull a sample (non-blocking)
    GstSample* sample = gst_app_sink_try_pull_sample(m_appSink, 0);
    if (sample) {
        // Update frame tracking for timeout detection
        m_lastFrameTime.store(QDateTime::currentMSecsSinceEpoch());
        m_frameCount.fetch_add(1);

        handleDecodedFrame(sample);
        gst_sample_unref(sample);
    }
}

void CameraController::onFrameTimeout() {
    if (!m_active.load()) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 lastFrame = m_lastFrameTime.load();
    qint64 elapsed = now - lastFrame;

    // If no frame received for more than 1 second, signal loss
    if (elapsed > 1000) {
        qWarning() << "[CameraController] Frame timeout - no frames for" << elapsed << "ms";

        if (!m_testMode) {
            // Real camera: likely disconnected or signal loss
            setStatus("Signal lost - reconnecting...");
            emit cameraError("Camera signal lost");

            // Try to recover
            stop();
            cleanupGStreamer();

            // Re-check and restart after delay
            QTimer::singleShot(1000, this, [this]() {
                if (checkAvailability()) {
                    start();
                }
            });
        } else {
            // Test mode: shouldn't happen, restart pipeline
            qWarning() << "[CameraController] Test mode frame timeout - restarting";
            stop();
            cleanupGStreamer();
            start();
        }
    }
}

void CameraController::handleDecodedFrame(GstSample* sample) {
    if (!sample) {
        return;
    }

    // Thread-safe access to video sink
    QVideoSink* sink = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        sink = m_videoSink;
    }

    if (!sink) {
        return;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps     = gst_sample_get_caps(sample);

    if (!buffer || !caps) {
        return;
    }

    // Get video info from caps
    GstVideoInfo videoInfo;
    if (!gst_video_info_from_caps(&videoInfo, caps)) {
        qWarning() << "[CameraController] Failed to get video info from caps";
        return;
    }

    int width  = GST_VIDEO_INFO_WIDTH(&videoInfo);
    int height = GST_VIDEO_INFO_HEIGHT(&videoInfo);

    // Map buffer for reading
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        qWarning() << "[CameraController] Failed to map buffer";
        return;
    }

    // Create QImage from the RGB data
    QImage image(map.data, width, height, GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0),
                 QImage::Format_RGB888);

    // Create a deep copy so we can unmap the GStreamer buffer
    QImage frameCopy = image.copy();

    gst_buffer_unmap(buffer, &map);

    // Create QVideoFrame from the image
    QVideoFrame frame(frameCopy);

    // Send frame to video sink (thread-safe: sink pointer is local copy)
    sink->setVideoFrame(frame);

    // Debug: log frame delivery periodically
    int count = m_frameCount.load();
    if (count % 150 == 1) {  // Every 5 seconds at 30fps
        qDebug() << "[CameraController] Frame delivered:" << width << "x" << height
                 << "frame#" << count << (m_testMode ? "(TEST)" : "");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// State management
// ═══════════════════════════════════════════════════════════════════════════════

void CameraController::setError(const QString& message) {
    m_errorMessage = message;
    emit errorChanged();
    qWarning() << "[CameraController] Error:" << message;
}

void CameraController::clearError() {
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorChanged();
    }
}

void CameraController::setStatus(const QString& message) {
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusChanged();
        qInfo() << "[CameraController] Status:" << message;
    }
}

void CameraController::setAvailable(bool available) {
    if (m_available.load() != available) {
        m_available.store(available);
        emit availableChanged();

        if (!available) {
            emit cameraDisconnected();
        }
    }
}

void CameraController::setActive(bool active) {
    if (m_active.load() != active) {
        m_active.store(active);
        emit activeChanged();
    }
}

}  // namespace speeduino

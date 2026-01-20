/**
 * @file qml_video_output.hpp
 * @brief GStreamer-based video output for OpenAuto with Qt6 QVideoSink
 *
 * This class implements openauto's IVideoOutput interface using GStreamer
 * for H.264 decoding and outputs to Qt6's QVideoSink for QML display.
 *
 * Architecture:
 * 1. OpenAuto calls write() with H.264 NAL units
 * 2. Data is pushed to GStreamer appsrc
 * 3. GStreamer pipeline: appsrc ! h264parse ! v4l2h264dec ! videoconvert ! appsink
 * 4. Decoded frames are pulled from appsink
 * 5. Frames are converted to QVideoFrame and sent to QVideoSink
 * 6. QML VideoOutput displays from the sink
 *
 * Copyright (C) 2024 Speeduino UI Project
 * License: GPL-3.0
 */

#ifndef HMI_QML_VIDEO_OUTPUT_HPP
#define HMI_QML_VIDEO_OUTPUT_HPP

#include <QObject>
#include <QVideoSink>
#include <QVideoFrame>
#include <QMutex>
#include <QTimer>
#include <memory>
#include <atomic>
#include <thread>

// GStreamer includes
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

// OpenAuto includes
#include <openauto/Projection/VideoOutput.hpp>
#include <openauto/Configuration/IConfiguration.hpp>
#include <aasdk/Common/Data.hpp>

namespace speeduino {

/**
 * @brief GStreamer-based video output for OpenAuto with Qt6 QML integration
 *
 * This class bridges OpenAuto's H.264 video stream to QML's VideoOutput element
 * using GStreamer for hardware-accelerated decoding on Raspberry Pi.
 *
 * Usage in QML:
 * @code
 * VideoOutput {
 *     id: videoOutput
 *     anchors.fill: parent
 * }
 *
 * // In C++, call setVideoSink(videoOutput.videoSink)
 * @endcode
 */
class QMLVideoOutput : public QObject, public openauto::projection::VideoOutput
{
    Q_OBJECT

    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)

public:
    using Pointer = std::shared_ptr<QMLVideoOutput>;

    /**
     * @brief Construct GStreamer-based video output
     * @param configuration OpenAuto configuration for resolution/FPS settings
     * @param parent QObject parent
     */
    explicit QMLVideoOutput(openauto::configuration::IConfiguration::Pointer configuration,
                            QObject* parent = nullptr);

    ~QMLVideoOutput() override;

    // ═══════════════════════════════════════════════════════════════════════
    // IVideoOutput interface implementation
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Open the video output for streaming
     * @return true if successfully opened
     */
    bool open() override;

    /**
     * @brief Initialize playback (called when projection starts)
     * @return true if successfully initialized
     */
    bool init() override;

    /**
     * @brief Write H.264 encoded video data
     * @param timestamp Presentation timestamp in microseconds
     * @param buffer Buffer containing H.264 NAL unit data
     *
     * This is called from OpenAuto's video service thread.
     * Data is pushed to GStreamer's appsrc for decoding.
     */
    void write(uint64_t timestamp, const aasdk::common::DataConstBuffer& buffer) override;

    /**
     * @brief Stop video playback
     */
    void stop() override;

    // ═══════════════════════════════════════════════════════════════════════
    // QML interface
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Get the video sink for QML VideoOutput binding
     */
    QVideoSink* videoSink() const {
        QMutexLocker locker(&m_mutex);
        return m_videoSink;
    }

    /**
     * @brief Set the video sink from QML VideoOutput
     * @param sink The QVideoSink from QML's VideoOutput.videoSink property
     */
    Q_INVOKABLE void setVideoSink(QVideoSink* sink);

    /**
     * @brief Check if video is currently playing
     */
    bool isPlaying() const { return m_playing.load(); }

signals:
    void videoSinkChanged();
    void playingChanged();
    void playbackStarted();
    void playbackStopped();
    void errorOccurred(const QString& error);

private slots:
    void doStartPlayback();
    void doStopPlayback();
    void processFrames();

private:
    bool initGStreamer();
    void cleanupGStreamer();
    QString findBestDecoder();

    // GStreamer callback for new samples
    static GstFlowReturn onNewSample(GstAppSink* sink, gpointer userData);
    static gboolean onBusMessage(GstBus* bus, GstMessage* message, gpointer userData);

    // Convert GStreamer buffer to QVideoFrame
    void handleDecodedFrame(GstSample* sample);

    // Video sink (owned by QML VideoOutput)
    QVideoSink* m_videoSink{nullptr};

    // GStreamer pipeline components
    GstElement* m_pipeline{nullptr};
    GstAppSrc* m_appSrc{nullptr};
    GstAppSink* m_appSink{nullptr};
    GstBus* m_bus{nullptr};

    // Frame processing timer
    QTimer* m_frameTimer{nullptr};

    // State
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_opened{false};
    std::atomic<bool> m_gstInitialized{false};

    // Thread safety
    mutable QMutex m_mutex;

    // Video dimensions (from OpenAuto configuration)
    int m_width{800};
    int m_height{480};
};

} // namespace speeduino

#endif // HMI_QML_VIDEO_OUTPUT_HPP

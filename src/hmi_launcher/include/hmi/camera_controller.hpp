/**
 * @file camera_controller.hpp
 * @brief GStreamer-based reverse camera controller with Qt6 QVideoSink output
 *
 * This class captures video from a V4L2 device (USB capture card or camera)
 * and outputs to Qt6's QVideoSink for QML display.
 *
 * Architecture:
 * 1. GStreamer pipeline: v4l2src ! videoconvert ! appsink
 * 2. Decoded frames are pulled from appsink
 * 3. Frames are converted to QVideoFrame and sent to QVideoSink
 * 4. QML VideoOutput displays from the sink
 *
 * Supports:
 * - USB capture cards (EasyCap, etc.) with composite video input
 * - Direct USB cameras (webcams)
 * - Hot-plug detection (graceful handling of missing camera)
 * - Low-latency configuration for automotive use
 *
 * Copyright (C) 2024 Speeduino UI Project
 * License: GPL-3.0
 */

#ifndef HMI_CAMERA_CONTROLLER_HPP
#define HMI_CAMERA_CONTROLLER_HPP

#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <atomic>

// GStreamer includes
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

namespace speeduino {

/**
 * @brief GStreamer-based camera controller for reverse camera with QML integration
 *
 * Usage in QML:
 * @code
 * VideoOutput {
 *     id: cameraOutput
 *     anchors.fill: parent
 * }
 *
 * Component.onCompleted: {
 *     cameraController.setVideoSink(cameraOutput.videoSink)
 * }
 * @endcode
 */
class CameraController : public QObject {
    Q_OBJECT

    // QML-accessible properties
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY deviceChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(int width READ width CONSTANT)
    Q_PROPERTY(int height READ height CONSTANT)
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    // Test/simulation mode properties
    Q_PROPERTY(bool testMode READ isTestMode WRITE setTestMode NOTIFY testModeChanged)
    Q_PROPERTY(QString testPattern READ testPattern WRITE setTestPattern NOTIFY testPatternChanged)

    // Parking guide line calibration (read from config, editable at runtime)
    Q_PROPERTY(double guideBottomWidth READ guideBottomWidth WRITE setGuideBottomWidth NOTIFY guideChanged)
    Q_PROPERTY(double guideTopWidth READ guideTopWidth WRITE setGuideTopWidth NOTIFY guideChanged)
    Q_PROPERTY(double guideBottomY READ guideBottomY WRITE setGuideBottomY NOTIFY guideChanged)
    Q_PROPERTY(double guideTopY READ guideTopY WRITE setGuideTopY NOTIFY guideChanged)
    Q_PROPERTY(double guideDistance1 READ guideDistance1 WRITE setGuideDistance1 NOTIFY guideChanged)
    Q_PROPERTY(double guideDistance2 READ guideDistance2 WRITE setGuideDistance2 NOTIFY guideChanged)
    Q_PROPERTY(double guideDistance3 READ guideDistance3 WRITE setGuideDistance3 NOTIFY guideChanged)
    Q_PROPERTY(bool showGuides READ showGuides WRITE setShowGuides NOTIFY guideChanged)

public:
    explicit CameraController(QObject* parent = nullptr);
    ~CameraController() override;

    // ═══════════════════════════════════════════════════════════════════════
    // Property getters
    // ═══════════════════════════════════════════════════════════════════════

    bool isAvailable() const { return m_available.load(); }
    bool isActive() const { return m_active.load(); }
    QString device() const { return m_device; }
    QString errorMessage() const { return m_errorMessage; }
    QString statusMessage() const { return m_statusMessage; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    QVideoSink* videoSink() const;
    bool isTestMode() const { return m_testMode; }
    QString testPattern() const { return m_testPattern; }

    // Parking guide line getters
    double guideBottomWidth() const { return m_guideBottomWidth; }
    double guideTopWidth() const { return m_guideTopWidth; }
    double guideBottomY() const { return m_guideBottomY; }
    double guideTopY() const { return m_guideTopY; }
    double guideDistance1() const { return m_guideDistance1; }
    double guideDistance2() const { return m_guideDistance2; }
    double guideDistance3() const { return m_guideDistance3; }
    bool showGuides() const { return m_showGuides; }

    // ═══════════════════════════════════════════════════════════════════════
    // Configuration (call before start)
    // ═══════════════════════════════════════════════════════════════════════

    Q_INVOKABLE void setDevice(const QString& device);
    Q_INVOKABLE void setResolution(int width, int height);
    Q_INVOKABLE void setFramerate(int fps);
    Q_INVOKABLE void setVideoStandard(const QString& standard);  // "PAL" or "NTSC"
    Q_INVOKABLE void setCompositeInput(int input);               // Usually 0, 1, or 2

    /**
     * @brief Set the video sink from QML VideoOutput
     * @param sink The QVideoSink from QML's VideoOutput.videoSink property
     */
    Q_INVOKABLE void setVideoSink(QVideoSink* sink);

    /**
     * @brief Enable/disable test mode (simulated camera)
     * @param enabled true to use test pattern instead of real camera
     *
     * When test mode is enabled, the controller generates a test video
     * pattern (ball, SMPTE bars, etc.) instead of capturing from a real
     * camera. Useful for development without hardware.
     */
    Q_INVOKABLE void setTestMode(bool enabled);

    /**
     * @brief Set the test pattern to use in test mode
     * @param pattern One of: "ball", "smpte", "snow", "checkers", "gradient"
     */
    Q_INVOKABLE void setTestPattern(const QString& pattern);

    /**
     * @brief Get list of available test patterns
     * @return List of pattern names
     */
    Q_INVOKABLE QStringList availableTestPatterns() const;

    // Parking guide line setters (for runtime calibration)
    Q_INVOKABLE void setGuideBottomWidth(double value);
    Q_INVOKABLE void setGuideTopWidth(double value);
    Q_INVOKABLE void setGuideBottomY(double value);
    Q_INVOKABLE void setGuideTopY(double value);
    Q_INVOKABLE void setGuideDistance1(double value);
    Q_INVOKABLE void setGuideDistance2(double value);
    Q_INVOKABLE void setGuideDistance3(double value);
    Q_INVOKABLE void setShowGuides(bool show);

    // ═══════════════════════════════════════════════════════════════════════
    // Control
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Check if camera device is available
     * @return true if device exists and can be opened
     */
    Q_INVOKABLE bool checkAvailability();

    /**
     * @brief Start camera capture and display
     * @return true if started successfully
     */
    Q_INVOKABLE bool start();

    /**
     * @brief Stop camera capture
     */
    Q_INVOKABLE void stop();

signals:
    void availableChanged();
    void activeChanged();
    void deviceChanged();
    void errorChanged();
    void statusChanged();
    void videoSinkChanged();
    void testModeChanged();
    void testPatternChanged();
    void guideChanged();  // Emitted when any guide property changes

    // High-level events
    void cameraReady();
    void cameraError(const QString& message);
    void cameraDisconnected();

private slots:
    void processFrames();
    void onDeviceCheckTimer();
    void onFrameTimeout();

private:
    // GStreamer setup
    bool initGStreamer();
    void cleanupGStreamer();
    QString buildPipelineString() const;

    // GStreamer callbacks
    static gboolean onBusMessage(GstBus* bus, GstMessage* message, gpointer userData);

    // Frame handling
    void handleDecodedFrame(GstSample* sample);

    // State management
    void setError(const QString& message);
    void clearError();
    void setStatus(const QString& message);
    void setAvailable(bool available);
    void setActive(bool active);

    // Test mode helpers
    int getTestPatternId(const QString& pattern) const;
    QString buildTestPipelineString() const;

    // ═══════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════

    QString m_device{"/dev/video0"};
    int m_width{720};
    int m_height{576};  // PAL default
    int m_fps{25};      // PAL default
    QString m_videoStandard{"PAL"};
    int m_compositeInput{0};  // Default composite input

    // Test/simulation mode
    bool m_testMode{false};
    QString m_testPattern{"ball"};  // Default: moving ball animation

    // Parking guide line configuration (percentages 0.0-1.0)
    double m_guideBottomWidth{0.8};   // Width at bottom of screen
    double m_guideTopWidth{0.4};      // Width at top/far end
    double m_guideBottomY{0.95};      // Y position of bottom line
    double m_guideTopY{0.45};         // Y position of top line
    double m_guideDistance1{0.5};     // Near marker distance (meters)
    double m_guideDistance2{1.0};     // Middle marker distance
    double m_guideDistance3{1.5};     // Far marker distance
    bool m_showGuides{true};          // Show/hide guide lines

    // ═══════════════════════════════════════════════════════════════════════
    // State
    // ═══════════════════════════════════════════════════════════════════════

    std::atomic<bool> m_available{false};
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_gstInitialized{false};

    QString m_errorMessage;
    QString m_statusMessage{"Initializing..."};

    // Error recovery
    int m_retryCount{0};
    static constexpr int MAX_RETRIES = 5;

    // ═══════════════════════════════════════════════════════════════════════
    // GStreamer components
    // ═══════════════════════════════════════════════════════════════════════

    GstElement* m_pipeline{nullptr};
    GstAppSink* m_appSink{nullptr};
    GstBus* m_bus{nullptr};

    // ═══════════════════════════════════════════════════════════════════════
    // Qt components
    // ═══════════════════════════════════════════════════════════════════════

    QVideoSink* m_videoSink{nullptr};
    QTimer* m_frameTimer{nullptr};
    QTimer* m_deviceCheckTimer{nullptr};
    QTimer* m_frameTimeoutTimer{nullptr};  // Detects signal loss

    // Frame delivery tracking
    std::atomic<int> m_frameCount{0};
    std::atomic<qint64> m_lastFrameTime{0};

    // Thread safety
    mutable QMutex m_mutex;
};

}  // namespace speeduino

#endif  // HMI_CAMERA_CONTROLLER_HPP

#ifndef HMI_OPENAUTO_EMBEDDED_HPP
#define HMI_OPENAUTO_EMBEDDED_HPP

#include <QObject>
#include <QWidget>
#include <QThread>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QMutex>
#include <QTimer>
#include <memory>
#include <functional>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "hmi/qml_video_output.hpp"

// Boost forward declarations
#include <boost/asio.hpp>
namespace aasdk {
    namespace usb {
        class USBWrapper;
        class IUSBHub;
        class IConnectedAccessoriesEnumerator;
        class AccessoryModeQueryFactory;
        class AccessoryModeQueryChainFactory;
    }
    namespace tcp { class ITCPWrapper; }
}
namespace openauto {
    class App;
    namespace service {
        class ServiceFactory;
        class IAndroidAutoEntityFactory;
    }
    namespace configuration { class Configuration; }
}
struct libusb_context;

namespace speeduino {

/**
 * @brief Worker thread for openauto io_service
 */
class OpenAutoIOWorker : public QObject {
    Q_OBJECT
public:
    explicit OpenAutoIOWorker(boost::asio::io_service& ioService);
    ~OpenAutoIOWorker();

public slots:
    void run();
    void stop();

signals:
    // FIX #10: Signal emitted when exception loop limit exceeded
    void fatalError(const QString& message);

private:
    boost::asio::io_service& m_ioService;
    std::atomic<bool> m_running{false};

    // FIX #10: Exception loop prevention constants
    static constexpr int MAX_CONSECUTIVE_EXCEPTIONS = 10;
    static constexpr int INITIAL_RETRY_DELAY_MS = 100;
    static constexpr int MAX_RETRY_DELAY_MS = 5000;
};

/**
 * @brief OpenAutoEmbedded provides embedded Android Auto integration using libopenauto
 *
 * This class directly uses libopenauto library to embed Android Auto video
 * inside the Qt application, rather than launching autoapp as a separate process.
 *
 * The video output is rendered to a QWidget that can be embedded in QML.
 *
 * ARCHITECTURE:
 * 1. QML creates an Item with objectName "openAutoVideoSurface"
 * 2. When OpenAutoScreen loads, QML calls setVideoContainer() with that Item
 * 3. C++ parents the video widget to the QML window and positions it over the Item
 * 4. When screen visibility changes, QML calls setVideoVisible()
 * 5. Touch events from QML MouseArea are forwarded via sendTouch()
 */
class OpenAutoEmbedded : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString phoneName READ phoneName NOTIFY phoneNameChanged)
    Q_PROPERTY(bool videoVisible READ isVideoVisible NOTIFY videoVisibleChanged)
    Q_PROPERTY(QMLVideoOutput* qmlVideoOutput READ qmlVideoOutput NOTIFY qmlVideoOutputChanged)

public:
    explicit OpenAutoEmbedded(QObject* parent = nullptr);
    ~OpenAutoEmbedded();

    // Property getters (thread-safe)
    bool isRunning() const;
    bool isConnected() const;
    QString errorMessage() const;
    QString phoneName() const;
    bool isVideoVisible() const;
    QMLVideoOutput* qmlVideoOutput() const { return m_qmlVideoOutput.get(); }

    // Control
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();

    // ═══════════════════════════════════════════════════════════════
    // VIDEO OUTPUT (QML-native approach)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Update video visibility based on screen state
     * @param visible Whether the OpenAutoScreen is currently visible
     *
     * Called from OpenAutoScreen.qml onVisibleChanged.
     * Controls video playback when navigating between screens.
     */
    Q_INVOKABLE void setVideoVisible(bool visible);

    // Touch/input forwarding
    Q_INVOKABLE void sendTouch(int x, int y, int action);
    Q_INVOKABLE void sendKey(int keyCode, bool pressed);

    // Configuration
    Q_INVOKABLE void setResolution(int width, int height);
    Q_INVOKABLE void setNightMode(bool nightMode);

    /**
     * @brief Retry device detection when external USB detection finds a device
     *
     * Called from main.cpp when OpenAutoController detects an Android Auto device.
     * This is a workaround for libusb hotplug not working reliably on all systems.
     * Triggers aasdk to re-enumerate USB devices and attempt connection.
     */
    Q_INVOKABLE void retryDeviceDetection();

signals:
    void runningChanged();
    void connectedChanged();
    void errorChanged();
    void phoneNameChanged();
    void videoVisibleChanged();
    void qmlVideoOutputChanged();

    void started();
    void stopped();
    void phoneConnected(const QString& deviceName);
    void phoneDisconnected();
    void projectionStarted();
    void projectionStopped();

private slots:
    void onProjectionActive(bool active);

private:
    bool initializeLibusb();
    void cleanupLibusb();
    bool initializeOpenauto();
    void cleanupOpenauto();
    void setError(const QString& msg);
    void setConnected(bool connected);

    // MISRA 15.6 FIX: Helper function for restart attempts (reduces nesting depth)
    void tryRestartAttempt(int attemptNumber, int delayMs);

    // Phase 1: USB worker synchronization helpers
    void stopUsbWorkersSync();  // Stops USB workers with proper synchronization

    // Phase 2: Graceful disconnect helper
    bool requestGracefulDisconnect(int timeoutMs);  // Request phone to disconnect cleanly

    // Thread synchronization - protects shared state accessed from multiple threads
    mutable QMutex m_stateMutex;

    // State (protected by m_stateMutex)
    bool m_running{false};
    bool m_connected{false};
    bool m_touchPressed{false};
    bool m_videoVisible{false};
    bool m_waitingForDevice{false};  // FIX: Prevent concurrent waitForDevice() calls
    bool m_pendingRetry{false};      // FIX: Queue retry if device detected before start()
    bool m_restarting{false};        // FIX: Debounce restart() to prevent multiple simultaneous restarts
    QString m_errorMessage;
    QString m_phoneName;
    int m_width{800};
    int m_height{480};
    bool m_nightMode{false};

    // QML-native video output (renders to QVideoSink for QML VideoOutput)
    std::shared_ptr<QMLVideoOutput> m_qmlVideoOutput;

    // Hidden input widget for InputDevice event filter
    // (required because openauto's InputDevice uses QWidget event filter)
    std::unique_ptr<QWidget> m_inputWidget;

    // libusb
    libusb_context* m_usbContext{nullptr};

    // boost::asio
    std::unique_ptr<boost::asio::io_service> m_ioService;
    std::unique_ptr<boost::asio::io_service::work> m_ioWork;  // Keeps io_service running
    std::unique_ptr<QThread> m_ioThread;
    std::unique_ptr<OpenAutoIOWorker> m_ioWorker;

    // USB worker threads - CRITICAL for processing libusb async transfers!
    // Without these, USB control transfers (AOA protocol) never complete.
    std::vector<std::thread> m_usbWorkerThreads;
    std::atomic<bool> m_usbWorkersRunning{false};

    // Phase 1: USB worker synchronization for clean shutdown
    std::mutex m_usbWorkerMutex;
    std::condition_variable m_usbWorkerCV;
    std::atomic<int> m_usbWorkersActive{0};  // Count of active USB workers

    // openauto components
    std::unique_ptr<aasdk::usb::USBWrapper> m_usbWrapper;
    std::unique_ptr<aasdk::tcp::ITCPWrapper> m_tcpWrapper;
    std::shared_ptr<openauto::configuration::Configuration> m_configuration;
    std::unique_ptr<openauto::service::ServiceFactory> m_serviceFactory;
    std::unique_ptr<openauto::service::IAndroidAutoEntityFactory> m_androidAutoEntityFactory;

    // FIX: Query factories MUST be member variables to persist for lifetime of USBHub/Enumerator
    // Previously these were local variables in initializeOpenauto() causing USE-AFTER-FREE!
    // USBHub and ConnectedAccessoriesEnumerator store REFERENCES to these factories.
    std::unique_ptr<aasdk::usb::AccessoryModeQueryFactory> m_queryFactory;
    std::unique_ptr<aasdk::usb::AccessoryModeQueryChainFactory> m_queryChainFactory;

    std::shared_ptr<aasdk::usb::IUSBHub> m_usbHub;
    std::shared_ptr<aasdk::usb::IConnectedAccessoriesEnumerator> m_connectedAccessoriesEnumerator;
    std::shared_ptr<openauto::App> m_app;
};

} // namespace speeduino

#endif // HMI_OPENAUTO_EMBEDDED_HPP

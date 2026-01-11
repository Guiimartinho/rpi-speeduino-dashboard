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

// Boost forward declarations
#include <boost/asio.hpp>
namespace aasdk {
    namespace usb {
        class USBWrapper;
        class IUSBHub;
        class IConnectedAccessoriesEnumerator;
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

private:
    boost::asio::io_service& m_ioService;
    std::atomic<bool> m_running{false};
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

public:
    explicit OpenAutoEmbedded(QObject* parent = nullptr);
    ~OpenAutoEmbedded();

    // Property getters (thread-safe)
    bool isRunning() const;
    bool isConnected() const;
    QString errorMessage() const;
    QString phoneName() const;
    bool isVideoVisible() const;
    QWidget* videoWidget() const { return m_videoWidget.get(); }

    // Control
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();

    // ═══════════════════════════════════════════════════════════════
    // VIDEO CONTAINER INTEGRATION (called from QML)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Register the QML container item where video should be displayed
     * @param container The QQuickItem that defines the video display area
     *
     * Called from OpenAutoScreen.qml Component.onCompleted.
     * This establishes the parent relationship and positions the video widget.
     */
    Q_INVOKABLE void setVideoContainer(QQuickItem* container);

    /**
     * @brief Update video widget visibility based on screen state
     * @param visible Whether the OpenAutoScreen is currently visible
     *
     * Called from OpenAutoScreen.qml onVisibleChanged.
     * Hides video when user navigates to other tabs, shows when returning.
     */
    Q_INVOKABLE void setVideoVisible(bool visible);

    /**
     * @brief Update video widget geometry when container resizes
     * @param x X position relative to window
     * @param y Y position relative to window
     * @param width Container width
     * @param height Container height
     *
     * Called from OpenAutoScreen.qml when geometry changes.
     */
    Q_INVOKABLE void updateVideoGeometry(int x, int y, int width, int height);

    // Touch/input forwarding
    Q_INVOKABLE void sendTouch(int x, int y, int action);
    Q_INVOKABLE void sendKey(int keyCode, bool pressed);

    // Configuration
    Q_INVOKABLE void setResolution(int width, int height);
    Q_INVOKABLE void setNightMode(bool nightMode);

signals:
    void runningChanged();
    void connectedChanged();
    void errorChanged();
    void phoneNameChanged();
    void videoVisibleChanged();

    void started();
    void stopped();
    void phoneConnected(const QString& deviceName);
    void phoneDisconnected();
    void projectionStarted();
    void projectionStopped();

private slots:
    void onProjectionActive(bool active);
    void onContainerGeometryChanged();
    void onContainerDestroyed();
    void onGeometryUpdateTimeout();

private:
    bool initializeLibusb();
    void cleanupLibusb();
    bool initializeOpenauto();
    void cleanupOpenauto();
    void setError(const QString& msg);
    void setConnected(bool connected);
    void updateVideoWidgetPosition();

    // Coordinate validation helpers (ISO 26262 defensive programming)
    static constexpr int MAX_COORDINATE = 10000;  // Reasonable display limit
    static constexpr int MIN_DIMENSION = 1;
    int safeCoordinate(qreal value, int minVal, int maxVal) const;
    bool validateGeometry(int x, int y, int w, int h) const;

    // Thread synchronization - protects shared state accessed from multiple threads
    mutable QMutex m_stateMutex;

    // State (protected by m_stateMutex)
    bool m_running{false};
    bool m_connected{false};
    bool m_touchPressed{false};
    bool m_videoVisible{false};
    bool m_containerRegistered{false};
    QString m_errorMessage;
    QString m_phoneName;
    int m_width{800};
    int m_height{480};
    bool m_nightMode{false};

    // Video widget for rendering
    std::unique_ptr<QWidget> m_videoWidget;

    // QML container reference (for geometry tracking)
    // Using QPointer to safely detect if container is destroyed
    QPointer<QQuickItem> m_container;
    QPointer<QQuickWindow> m_containerWindow;

    // Geometry update debounce timer (prevents excessive updates during animations)
    std::unique_ptr<QTimer> m_geometryUpdateTimer;

    // libusb
    libusb_context* m_usbContext{nullptr};

    // boost::asio
    std::unique_ptr<boost::asio::io_service> m_ioService;
    std::unique_ptr<boost::asio::io_service::work> m_ioWork;  // Keeps io_service running
    std::unique_ptr<QThread> m_ioThread;
    std::unique_ptr<OpenAutoIOWorker> m_ioWorker;

    // openauto components
    std::unique_ptr<aasdk::usb::USBWrapper> m_usbWrapper;
    std::unique_ptr<aasdk::tcp::ITCPWrapper> m_tcpWrapper;
    std::shared_ptr<openauto::configuration::Configuration> m_configuration;
    std::unique_ptr<openauto::service::ServiceFactory> m_serviceFactory;
    std::unique_ptr<openauto::service::IAndroidAutoEntityFactory> m_androidAutoEntityFactory;
    std::shared_ptr<aasdk::usb::IUSBHub> m_usbHub;
    std::shared_ptr<aasdk::usb::IConnectedAccessoriesEnumerator> m_connectedAccessoriesEnumerator;
    std::shared_ptr<openauto::App> m_app;
};

} // namespace speeduino

#endif // HMI_OPENAUTO_EMBEDDED_HPP

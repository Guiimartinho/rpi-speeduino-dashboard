#include "hmi/openauto_embedded.hpp"
#include <QDebug>
#include <QApplication>
#include <QTimer>
#include <QMouseEvent>
#include <QWindow>

// libusb
#include <libusb-1.0/libusb.h>

// boost
#include <boost/asio.hpp>

// Temporarily undefine Qt's emit macro to avoid conflict with std::syncstream
#ifdef emit
#undef emit
#define SPEEDUINO_EMIT_WAS_DEFINED
#endif

// aasdk
#include <aasdk/USB/USBWrapper.hpp>
#include <aasdk/USB/USBHub.hpp>
#include <aasdk/USB/ConnectedAccessoriesEnumerator.hpp>
#include <aasdk/USB/AccessoryModeQueryFactory.hpp>
#include <aasdk/USB/AccessoryModeQueryChainFactory.hpp>
#include <aasdk/TCP/TCPWrapper.hpp>

// openauto
#include <openauto/App.hpp>
#include <openauto/Configuration/Configuration.hpp>
#include <openauto/Service/ServiceFactory.hpp>
#include <openauto/Service/AndroidAutoEntityFactory.hpp>

// Restore emit macro if it was defined
#ifdef SPEEDUINO_EMIT_WAS_DEFINED
#define emit
#undef SPEEDUINO_EMIT_WAS_DEFINED
#endif

namespace speeduino {

// Touch action constants (matching QML touchEvent signal)
constexpr int TOUCH_ACTION_PRESS = 0;
constexpr int TOUCH_ACTION_RELEASE = 1;
constexpr int TOUCH_ACTION_MOVE = 2;

// ═══════════════════════════════════════════════════════════════
// OpenAutoIOWorker implementation
// ═══════════════════════════════════════════════════════════════

OpenAutoIOWorker::OpenAutoIOWorker(boost::asio::io_service& ioService)
    : m_ioService(ioService)
{
}

OpenAutoIOWorker::~OpenAutoIOWorker()
{
    stop();
}

void OpenAutoIOWorker::run()
{
    m_running = true;
    qInfo() << "[OpenAutoEmbedded] IO worker thread started";

    while (m_running) {
        try {
            m_ioService.run();
            if (m_running) {
                m_ioService.reset();
            }
        } catch (const std::exception& e) {
            qWarning() << "[OpenAutoEmbedded] IO service exception:" << e.what();
        }
    }

    qInfo() << "[OpenAutoEmbedded] IO worker thread stopped";
}

void OpenAutoIOWorker::stop()
{
    m_running = false;
    m_ioService.stop();
}

// ═══════════════════════════════════════════════════════════════
// OpenAutoEmbedded implementation
// ═══════════════════════════════════════════════════════════════

OpenAutoEmbedded::OpenAutoEmbedded(QObject* parent)
    : QObject(parent)
{
    // Create the video widget that will be used for Android Auto video output
    // This widget will be parented to the QML window when setVideoContainer() is called
    m_videoWidget = std::make_unique<QWidget>();
    m_videoWidget->setMinimumSize(m_width, m_height);
    m_videoWidget->resize(m_width, m_height);
    m_videoWidget->setAttribute(Qt::WA_AcceptTouchEvents);
    m_videoWidget->setFocusPolicy(Qt::StrongFocus);

    // Initially hidden until container is registered and projection starts
    m_videoWidget->hide();

    qInfo() << "[OpenAutoEmbedded] Created with video widget" << m_width << "x" << m_height;
}

OpenAutoEmbedded::~OpenAutoEmbedded()
{
    // Hide video widget first to avoid visual glitches during destruction
    if (m_videoWidget) {
        m_videoWidget->hide();
    }
    stop();
}

// ═══════════════════════════════════════════════════════════════
// VIDEO CONTAINER INTEGRATION
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::setVideoContainer(QQuickItem* container)
{
    if (!container) {
        qWarning() << "[OpenAutoEmbedded] setVideoContainer called with null container";
        return;
    }

    QQuickWindow* window = container->window();
    if (!window) {
        qWarning() << "[OpenAutoEmbedded] Container has no window, deferring...";
        // Try again when the container gets a window (use UniqueConnection to avoid duplicates)
        connect(container, &QQuickItem::windowChanged, this, [this, container](QQuickWindow* win) {
            if (win) {
                setVideoContainer(container);
            }
        }, Qt::UniqueConnection);
        return;
    }

    // Handle container destruction - null our reference to avoid dangling pointer
    connect(container, &QObject::destroyed, this, [this]() {
        qWarning() << "[OpenAutoEmbedded] Container was destroyed";
        m_container = nullptr;
        m_containerWindow = nullptr;
        m_containerRegistered = false;
        if (m_videoWidget) {
            m_videoWidget->hide();
        }
    }, Qt::UniqueConnection);

    m_container = container;
    m_containerWindow = window;
    m_containerRegistered = true;

    qInfo() << "[OpenAutoEmbedded] Video container registered:"
            << "size" << container->width() << "x" << container->height()
            << "window" << window->width() << "x" << window->height();

    // Make video widget a native child window of the QML window
    m_videoWidget->setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    m_videoWidget->setAttribute(Qt::WA_NativeWindow, true);
    m_videoWidget->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

    // Force native window creation
    m_videoWidget->winId();
    QWindow* videoWindow = m_videoWidget->windowHandle();

    if (videoWindow) {
        // Parent to QML window - this embeds the widget
        videoWindow->setParent(window);
        qInfo() << "[OpenAutoEmbedded] Video widget parented to QML window";
    } else {
        qWarning() << "[OpenAutoEmbedded] Failed to get video widget window handle";
    }

    // Connect to container geometry changes (use UniqueConnection to avoid duplicates on re-registration)
    connect(container, &QQuickItem::xChanged, this, &OpenAutoEmbedded::onContainerGeometryChanged, Qt::UniqueConnection);
    connect(container, &QQuickItem::yChanged, this, &OpenAutoEmbedded::onContainerGeometryChanged, Qt::UniqueConnection);
    connect(container, &QQuickItem::widthChanged, this, &OpenAutoEmbedded::onContainerGeometryChanged, Qt::UniqueConnection);
    connect(container, &QQuickItem::heightChanged, this, &OpenAutoEmbedded::onContainerGeometryChanged, Qt::UniqueConnection);

    // Initial position update
    updateVideoWidgetPosition();

    // If projection is already active, show the widget
    if (m_connected && m_videoVisible) {
        m_videoWidget->show();
        m_videoWidget->raise();
    }
}

void OpenAutoEmbedded::setVideoVisible(bool visible)
{
    if (m_videoVisible == visible) {
        return;
    }

    m_videoVisible = visible;
    qInfo() << "[OpenAutoEmbedded] Video visibility:" << visible;

    if (!m_containerRegistered) {
        qDebug() << "[OpenAutoEmbedded] Container not registered yet, visibility will be applied later";
        emit videoVisibleChanged();
        return;
    }

    if (visible) {
        // Update position before showing
        updateVideoWidgetPosition();

        // Only show if projection is active
        if (m_connected) {
            m_videoWidget->show();
            m_videoWidget->raise();
            qInfo() << "[OpenAutoEmbedded] Video widget shown (projection active)";
        } else {
            qDebug() << "[OpenAutoEmbedded] Video visible but projection not active, widget hidden";
        }
    } else {
        m_videoWidget->hide();
        qInfo() << "[OpenAutoEmbedded] Video widget hidden (screen not visible)";
    }

    emit videoVisibleChanged();
}

void OpenAutoEmbedded::updateVideoGeometry(int x, int y, int width, int height)
{
    if (!m_videoWidget) {
        return;
    }

    // Validate geometry to avoid invalid values
    if (width <= 0 || height <= 0) {
        qWarning() << "[OpenAutoEmbedded] Invalid geometry ignored:" << width << "x" << height;
        return;
    }

    m_videoWidget->setGeometry(x, y, width, height);

    if (m_serviceFactory) {
        m_serviceFactory->resize();
    }

    qDebug() << "[OpenAutoEmbedded] Video geometry updated:" << x << y << width << "x" << height;
}

void OpenAutoEmbedded::onContainerGeometryChanged()
{
    updateVideoWidgetPosition();
}

void OpenAutoEmbedded::updateVideoWidgetPosition()
{
    if (!m_container || !m_videoWidget) {
        return;
    }

    // Map container position to scene coordinates
    QPointF scenePos = m_container->mapToScene(QPointF(0, 0));

    int x = static_cast<int>(scenePos.x());
    int y = static_cast<int>(scenePos.y());
    int w = static_cast<int>(m_container->width());
    int h = static_cast<int>(m_container->height());

    m_videoWidget->setGeometry(x, y, w, h);

    qDebug() << "[OpenAutoEmbedded] Video widget positioned at" << x << y << "size" << w << "x" << h;
}

// ═══════════════════════════════════════════════════════════════
// CONTROL METHODS
// ═══════════════════════════════════════════════════════════════

bool OpenAutoEmbedded::start()
{
    if (m_running) {
        qWarning() << "[OpenAutoEmbedded] Already running";
        return true;
    }

    qInfo() << "[OpenAutoEmbedded] Starting...";

    // Initialize libusb
    if (!initializeLibusb()) {
        return false;
    }

    // Initialize openauto components
    if (!initializeOpenauto()) {
        cleanupLibusb();
        return false;
    }

    m_running = true;

    // Clear any previous error message on successful start
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorChanged();
    }

    emit runningChanged();
    emit started();

    qInfo() << "[OpenAutoEmbedded] Started successfully, waiting for device...";

    // Start waiting for USB device connection
    m_app->waitForDevice(true);

    return true;
}

void OpenAutoEmbedded::stop()
{
    if (!m_running) {
        return;
    }

    qInfo() << "[OpenAutoEmbedded] Stopping...";

    // Stop the app first
    if (m_app) {
        m_app->stop();
    }

    // Cleanup openauto
    cleanupOpenauto();

    // Cleanup libusb
    cleanupLibusb();

    m_running = false;
    setConnected(false);
    emit runningChanged();
    emit stopped();

    qInfo() << "[OpenAutoEmbedded] Stopped";
}

void OpenAutoEmbedded::restart()
{
    qInfo() << "[OpenAutoEmbedded] Restarting...";
    stop();
    QTimer::singleShot(500, this, &OpenAutoEmbedded::start);
}

// ═══════════════════════════════════════════════════════════════
// INPUT HANDLING
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::sendTouch(int x, int y, int action)
{
    if (!m_running || !m_connected) {
        return;
    }

    if (!m_videoWidget) {
        qWarning() << "[OpenAutoEmbedded] Video widget is null, cannot send touch";
        return;
    }

    // The openauto InputDevice class is installed as an event filter on the video widget.
    // It intercepts touch/mouse events and forwards them to Android Auto.
    // We synthesize mouse events here which will be captured by the InputDevice eventFilter.

    QPointF localPos(x, y);

    // Calculate global position (only valid if widget is properly parented)
    QPointF globalPos = localPos;
    if (m_videoWidget->parentWidget()) {
        QPoint gp = m_videoWidget->mapToGlobal(QPoint(x, y));
        globalPos = QPointF(gp.x(), gp.y());
    }

    QEvent::Type eventType;
    Qt::MouseButton button = Qt::LeftButton;
    Qt::MouseButtons buttons;

    switch (action) {
        case TOUCH_ACTION_PRESS:
            eventType = QEvent::MouseButtonPress;
            buttons = Qt::LeftButton;
            m_touchPressed = true;
            break;
        case TOUCH_ACTION_RELEASE:
            eventType = QEvent::MouseButtonRelease;
            buttons = Qt::NoButton;
            m_touchPressed = false;
            break;
        case TOUCH_ACTION_MOVE:
            eventType = QEvent::MouseMove;
            buttons = m_touchPressed ? Qt::LeftButton : Qt::NoButton;
            button = Qt::NoButton;  // No button for move events
            break;
        default:
            qWarning() << "[OpenAutoEmbedded] Unknown touch action:" << action;
            return;
    }

    // Create and post the mouse event to the video widget
    // The InputDevice eventFilter will intercept it and forward to Android Auto
    QMouseEvent* mouseEvent = new QMouseEvent(
        eventType,
        localPos,
        globalPos,
        button,
        buttons,
        Qt::NoModifier
    );

    // Post the event to the video widget (will be handled by InputDevice eventFilter)
    QCoreApplication::postEvent(m_videoWidget.get(), mouseEvent);

    qDebug() << "[OpenAutoEmbedded] Touch event posted:" << x << y << "action:" << action;
}

void OpenAutoEmbedded::sendKey(int keyCode, bool pressed)
{
    if (!m_running || !m_connected) {
        return;
    }

    if (!m_videoWidget) {
        qWarning() << "[OpenAutoEmbedded] Video widget is null, cannot send key";
        return;
    }

    // Create and post key event to the video widget
    QEvent::Type eventType = pressed ? QEvent::KeyPress : QEvent::KeyRelease;

    QKeyEvent* keyEvent = new QKeyEvent(
        eventType,
        keyCode,
        Qt::NoModifier
    );

    QCoreApplication::postEvent(m_videoWidget.get(), keyEvent);

    qDebug() << "[OpenAutoEmbedded] Key event posted:" << keyCode << "pressed:" << pressed;
}

// ═══════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::setResolution(int width, int height)
{
    m_width = width;
    m_height = height;
    if (m_videoWidget) {
        m_videoWidget->setMinimumSize(width, height);
    }
    if (m_serviceFactory) {
        m_serviceFactory->resize();
    }
    qInfo() << "[OpenAutoEmbedded] Resolution set to" << width << "x" << height;
}

void OpenAutoEmbedded::setNightMode(bool nightMode)
{
    m_nightMode = nightMode;
    if (m_serviceFactory) {
        m_serviceFactory->setNightMode(nightMode);
    }
    qInfo() << "[OpenAutoEmbedded] Night mode:" << nightMode;
}

// ═══════════════════════════════════════════════════════════════
// PROJECTION LIFECYCLE
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::onProjectionActive(bool active)
{
    qInfo() << "[OpenAutoEmbedded] Projection active:" << active;

    if (active) {
        setConnected(true);
        emit projectionStarted();

        // Show video widget if screen is visible
        if (m_videoVisible && m_containerRegistered) {
            updateVideoWidgetPosition();
            m_videoWidget->show();
            m_videoWidget->raise();
            qInfo() << "[OpenAutoEmbedded] Video widget shown (projection started)";
        }
    } else {
        // Hide video widget
        if (m_videoWidget) {
            m_videoWidget->hide();
        }

        setConnected(false);
        emit projectionStopped();
    }
}

// ═══════════════════════════════════════════════════════════════
// INITIALIZATION / CLEANUP
// ═══════════════════════════════════════════════════════════════

bool OpenAutoEmbedded::initializeLibusb()
{
    int ret = libusb_init(&m_usbContext);
    if (ret != 0) {
        setError(QString("Failed to initialize libusb: %1").arg(libusb_error_name(ret)));
        return false;
    }

    qInfo() << "[OpenAutoEmbedded] libusb initialized";
    return true;
}

void OpenAutoEmbedded::cleanupLibusb()
{
    if (m_usbContext) {
        libusb_exit(m_usbContext);
        m_usbContext = nullptr;
        qInfo() << "[OpenAutoEmbedded] libusb cleaned up";
    }
}

bool OpenAutoEmbedded::initializeOpenauto()
{
    try {
        // Create boost::asio io_service
        m_ioService = std::make_unique<boost::asio::io_service>();

        // Create work guard to keep io_service running (stored as member to persist)
        m_ioWork = std::make_unique<boost::asio::io_service::work>(*m_ioService);

        // Create configuration
        m_configuration = std::make_shared<openauto::configuration::Configuration>();

        // Create TCP wrapper
        m_tcpWrapper = std::make_unique<aasdk::tcp::TCPWrapper>();

        // Create USB wrapper
        m_usbWrapper = std::make_unique<aasdk::usb::USBWrapper>(m_usbContext);

        // Create accessory mode query factories
        aasdk::usb::AccessoryModeQueryFactory queryFactory(*m_usbWrapper, *m_ioService);
        aasdk::usb::AccessoryModeQueryChainFactory queryChainFactory(*m_usbWrapper, *m_ioService, queryFactory);

        // Create ServiceFactory with our video widget as the activeArea
        // The callback will be called when projection starts/stops
        auto activeCallback = [this](bool active) {
            QMetaObject::invokeMethod(this, "onProjectionActive", Qt::QueuedConnection,
                                      Q_ARG(bool, active));
        };

        m_serviceFactory = std::make_unique<openauto::service::ServiceFactory>(
            *m_ioService,
            m_configuration,
            m_videoWidget.get(),  // Video renders to this widget
            activeCallback,
            m_nightMode
        );

        // Create AndroidAutoEntityFactory
        m_androidAutoEntityFactory = std::make_unique<openauto::service::AndroidAutoEntityFactory>(
            *m_ioService,
            m_configuration,
            *m_serviceFactory
        );

        // Create USB hub
        m_usbHub = std::make_shared<aasdk::usb::USBHub>(
            *m_usbWrapper,
            *m_ioService,
            queryChainFactory
        );

        // Create connected accessories enumerator
        m_connectedAccessoriesEnumerator = std::make_shared<aasdk::usb::ConnectedAccessoriesEnumerator>(
            *m_usbWrapper,
            *m_ioService,
            queryChainFactory
        );

        // Create the main App
        m_app = std::make_shared<openauto::App>(
            *m_ioService,
            *m_usbWrapper,
            *m_tcpWrapper,
            *m_androidAutoEntityFactory,
            std::move(m_usbHub),
            std::move(m_connectedAccessoriesEnumerator)
        );

        // Start IO worker thread
        m_ioThread = std::make_unique<QThread>();
        m_ioWorker = std::make_unique<OpenAutoIOWorker>(*m_ioService);
        m_ioWorker->moveToThread(m_ioThread.get());

        connect(m_ioThread.get(), &QThread::started, m_ioWorker.get(), &OpenAutoIOWorker::run);
        connect(m_ioThread.get(), &QThread::finished, m_ioWorker.get(), &QObject::deleteLater);

        m_ioThread->start();

        qInfo() << "[OpenAutoEmbedded] OpenAuto components initialized";
        return true;

    } catch (const std::exception& e) {
        setError(QString("Failed to initialize OpenAuto: %1").arg(e.what()));
        return false;
    }
}

void OpenAutoEmbedded::cleanupOpenauto()
{
    // Stop IO worker first
    if (m_ioWorker) {
        m_ioWorker->stop();
    }

    // Release work guard to allow io_service to stop
    m_ioWork.reset();

    // Stop IO thread and wait for it to finish
    if (m_ioThread && m_ioThread->isRunning()) {
        m_ioThread->quit();
        if (!m_ioThread->wait(3000)) {
            qWarning() << "[OpenAutoEmbedded] IO thread did not stop in time, terminating";
            m_ioThread->terminate();
            m_ioThread->wait();
        }
    }

    // CRITICAL: Release ownership of worker - Qt's deleteLater will delete it
    // If we don't release, unique_ptr destructor would cause double-free
    if (m_ioWorker) {
        m_ioWorker.release();
    }

    // Clear app and components in reverse order of creation
    m_app.reset();
    m_connectedAccessoriesEnumerator.reset();
    m_usbHub.reset();
    m_androidAutoEntityFactory.reset();
    m_serviceFactory.reset();
    m_tcpWrapper.reset();
    m_usbWrapper.reset();
    m_configuration.reset();

    // CRITICAL: Reset thread BEFORE io_service
    // The worker is already deleted by deleteLater when thread finished, so don't reset it
    // (m_ioWorker.reset() would cause double-free)
    m_ioThread.reset();

    // Now safe to reset io_service (no references remain)
    m_ioService.reset();

    qInfo() << "[OpenAutoEmbedded] OpenAuto components cleaned up";
}

void OpenAutoEmbedded::setError(const QString& msg)
{
    if (m_errorMessage != msg) {
        m_errorMessage = msg;
        emit errorChanged();
        qWarning() << "[OpenAutoEmbedded] Error:" << msg;
    }
}

void OpenAutoEmbedded::setConnected(bool connected)
{
    if (m_connected != connected) {
        m_connected = connected;
        emit connectedChanged();

        if (connected) {
            qInfo() << "[OpenAutoEmbedded] Phone connected";
            emit phoneConnected(m_phoneName);
        } else {
            m_phoneName.clear();
            emit phoneNameChanged();
            emit phoneDisconnected();
            qInfo() << "[OpenAutoEmbedded] Phone disconnected";
        }
    }
}

} // namespace speeduino

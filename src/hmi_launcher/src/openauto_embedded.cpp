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
    m_running.store(true, std::memory_order_release);
    qInfo() << "[OpenAutoEmbedded] IO worker thread started";

    while (m_running.load(std::memory_order_acquire)) {
        try {
            m_ioService.run();
            if (m_running.load(std::memory_order_acquire)) {
                m_ioService.reset();
            }
        } catch (const std::exception& e) {
            qWarning() << "[OpenAutoEmbedded] IO service exception:" << e.what();
            // Brief delay before retry to prevent tight error loop
            QThread::msleep(100);
        }
    }

    qInfo() << "[OpenAutoEmbedded] IO worker thread stopped";
}

void OpenAutoIOWorker::stop()
{
    m_running.store(false, std::memory_order_release);
    m_ioService.stop();
}

// ═══════════════════════════════════════════════════════════════
// OpenAutoEmbedded implementation
// ═══════════════════════════════════════════════════════════════

OpenAutoEmbedded::OpenAutoEmbedded(QObject* parent)
    : QObject(parent)
{
    // Create geometry update debounce timer
    m_geometryUpdateTimer = std::make_unique<QTimer>();
    m_geometryUpdateTimer->setSingleShot(true);
    m_geometryUpdateTimer->setInterval(16);  // ~60fps debounce
    connect(m_geometryUpdateTimer.get(), &QTimer::timeout,
            this, &OpenAutoEmbedded::onGeometryUpdateTimeout);

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

// ═══════════════════════════════════════════════════════════════
// THREAD-SAFE PROPERTY GETTERS
// ═══════════════════════════════════════════════════════════════

bool OpenAutoEmbedded::isRunning() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_running;
}

bool OpenAutoEmbedded::isConnected() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_connected;
}

QString OpenAutoEmbedded::errorMessage() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_errorMessage;
}

QString OpenAutoEmbedded::phoneName() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_phoneName;
}

bool OpenAutoEmbedded::isVideoVisible() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_videoVisible;
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
        // Use QPointer to avoid dangling pointer in lambda if container is destroyed
        QPointer<QQuickItem> weakContainer(container);
        connect(container, &QQuickItem::windowChanged, this, [this, weakContainer](QQuickWindow* win) {
            // Safety check: container still exists and has window
            if (weakContainer && win) {
                setVideoContainer(weakContainer.data());
            }
        }, Qt::UniqueConnection);
        return;
    }

    // Handle container destruction - use proper slot instead of lambda with this capture
    connect(container, &QObject::destroyed, this,
            &OpenAutoEmbedded::onContainerDestroyed, Qt::UniqueConnection);

    {
        QMutexLocker locker(&m_stateMutex);
        m_container = container;
        m_containerWindow = window;
        m_containerRegistered = true;
    }

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
    bool shouldShow = false;
    {
        QMutexLocker locker(&m_stateMutex);
        shouldShow = m_connected && m_videoVisible;
    }
    if (shouldShow) {
        m_videoWidget->show();
        m_videoWidget->raise();
    }
}

void OpenAutoEmbedded::onContainerDestroyed()
{
    qWarning() << "[OpenAutoEmbedded] Container was destroyed";
    {
        QMutexLocker locker(&m_stateMutex);
        m_container = nullptr;
        m_containerWindow = nullptr;
        m_containerRegistered = false;
    }
    if (m_videoWidget) {
        m_videoWidget->hide();
    }
}

void OpenAutoEmbedded::onGeometryUpdateTimeout()
{
    updateVideoWidgetPosition();
}

void OpenAutoEmbedded::setVideoVisible(bool visible)
{
    bool wasVisible;
    bool isConnected;
    bool isRegistered;

    {
        QMutexLocker locker(&m_stateMutex);
        wasVisible = m_videoVisible;
        if (m_videoVisible == visible) {
            return;
        }
        m_videoVisible = visible;
        isConnected = m_connected;
        isRegistered = m_containerRegistered;
    }

    qInfo() << "[OpenAutoEmbedded] Video visibility:" << visible;

    if (!isRegistered) {
        qDebug() << "[OpenAutoEmbedded] Container not registered yet, visibility will be applied later";
        emit videoVisibleChanged();
        return;
    }

    if (visible) {
        // Update position before showing
        updateVideoWidgetPosition();

        // Only show if projection is active
        if (isConnected) {
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

    // ISO 26262 defensive programming: validate all geometry parameters
    if (!validateGeometry(x, y, width, height)) {
        qWarning() << "[OpenAutoEmbedded] Invalid geometry rejected:"
                   << x << y << width << "x" << height;
        return;
    }

    // Apply safe bounds
    const int safeX = safeCoordinate(x, 0, MAX_COORDINATE);
    const int safeY = safeCoordinate(y, 0, MAX_COORDINATE);
    const int safeW = safeCoordinate(width, MIN_DIMENSION, MAX_COORDINATE);
    const int safeH = safeCoordinate(height, MIN_DIMENSION, MAX_COORDINATE);

    m_videoWidget->setGeometry(safeX, safeY, safeW, safeH);

    if (m_serviceFactory) {
        m_serviceFactory->resize();
    }

    qDebug() << "[OpenAutoEmbedded] Video geometry updated:" << safeX << safeY << safeW << "x" << safeH;
}

void OpenAutoEmbedded::onContainerGeometryChanged()
{
    // Use debounce timer to batch rapid geometry changes (animations, resize)
    // This prevents excessive position updates that could cause flicker
    if (m_geometryUpdateTimer && !m_geometryUpdateTimer->isActive()) {
        m_geometryUpdateTimer->start();
    }
}

void OpenAutoEmbedded::updateVideoWidgetPosition()
{
    // Thread-safe container access
    QPointer<QQuickItem> container;
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_containerRegistered || !m_container) {
            return;
        }
        container = m_container;
    }

    if (!m_videoWidget || !container) {
        return;
    }

    // Map container position to scene coordinates
    QPointF scenePos = container->mapToScene(QPointF(0, 0));

    // ISO 26262: Validate floating point values before integer conversion
    if (!std::isfinite(scenePos.x()) || !std::isfinite(scenePos.y()) ||
        !std::isfinite(container->width()) || !std::isfinite(container->height())) {
        qWarning() << "[OpenAutoEmbedded] Invalid container geometry (non-finite values)";
        return;
    }

    // Apply safe coordinate conversion with bounds checking
    const int x = safeCoordinate(scenePos.x(), 0, MAX_COORDINATE);
    const int y = safeCoordinate(scenePos.y(), 0, MAX_COORDINATE);
    const int w = safeCoordinate(container->width(), MIN_DIMENSION, MAX_COORDINATE);
    const int h = safeCoordinate(container->height(), MIN_DIMENSION, MAX_COORDINATE);

    // Final validation before applying
    if (!validateGeometry(x, y, w, h)) {
        qWarning() << "[OpenAutoEmbedded] Container geometry validation failed";
        return;
    }

    m_videoWidget->setGeometry(x, y, w, h);

    qDebug() << "[OpenAutoEmbedded] Video widget positioned at" << x << y << "size" << w << "x" << h;
}

// ═══════════════════════════════════════════════════════════════
// CONTROL METHODS
// ═══════════════════════════════════════════════════════════════

bool OpenAutoEmbedded::start()
{
    // Thread-safe running check
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_running) {
            qWarning() << "[OpenAutoEmbedded] Already running";
            return true;
        }
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

    // Thread-safe state update
    {
        QMutexLocker locker(&m_stateMutex);
        m_running = true;

        // Clear any previous error message on successful start
        if (!m_errorMessage.isEmpty()) {
            m_errorMessage.clear();
        }
    }

    emit errorChanged();
    emit runningChanged();
    emit started();

    qInfo() << "[OpenAutoEmbedded] Started successfully, waiting for device...";

    // Start waiting for USB device connection
    if (m_app) {
        m_app->waitForDevice(true);
    }

    return true;
}

void OpenAutoEmbedded::stop()
{
    // Thread-safe running check
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_running) {
            return;
        }
    }

    qInfo() << "[OpenAutoEmbedded] Stopping...";

    // Stop the app first (outside mutex to avoid deadlock with callbacks)
    if (m_app) {
        m_app->stop();
    }

    // Cleanup openauto
    cleanupOpenauto();

    // Cleanup libusb
    cleanupLibusb();

    // Thread-safe state update
    {
        QMutexLocker locker(&m_stateMutex);
        m_running = false;
    }

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
    // Thread-safe state check
    bool isRunning;
    bool isConnected;
    {
        QMutexLocker locker(&m_stateMutex);
        isRunning = m_running;
        isConnected = m_connected;
    }

    if (!isRunning || !isConnected) {
        return;
    }

    if (!m_videoWidget) {
        qWarning() << "[OpenAutoEmbedded] Video widget is null, cannot send touch";
        return;
    }

    // ISO 26262: Validate touch coordinates
    if (x < 0 || y < 0 || x > MAX_COORDINATE || y > MAX_COORDINATE) {
        qWarning() << "[OpenAutoEmbedded] Touch coordinates out of bounds:" << x << y;
        return;
    }

    // The openauto InputDevice class is installed as an event filter on the video widget.
    // It intercepts touch/mouse events and forwards them to Android Auto.
    // We synthesize mouse events here which will be captured by the InputDevice eventFilter.

    // Safe coordinate clamping for extra safety
    const int safeX = safeCoordinate(x, 0, m_videoWidget->width());
    const int safeY = safeCoordinate(y, 0, m_videoWidget->height());

    QPointF localPos(safeX, safeY);

    // Calculate global position (only valid if widget is properly parented)
    QPointF globalPos = localPos;
    if (m_videoWidget->parentWidget()) {
        QPoint gp = m_videoWidget->mapToGlobal(QPoint(safeX, safeY));
        globalPos = QPointF(gp.x(), gp.y());
    }

    QEvent::Type eventType;
    Qt::MouseButton button = Qt::LeftButton;
    Qt::MouseButtons buttons;

    // Thread-safe touch state access
    bool wasTouchPressed;
    {
        QMutexLocker locker(&m_stateMutex);
        wasTouchPressed = m_touchPressed;

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

    qDebug() << "[OpenAutoEmbedded] Touch event posted:" << safeX << safeY << "action:" << action;
}

void OpenAutoEmbedded::sendKey(int keyCode, bool pressed)
{
    // Thread-safe state check
    bool isRunning;
    bool isConnected;
    {
        QMutexLocker locker(&m_stateMutex);
        isRunning = m_running;
        isConnected = m_connected;
    }

    if (!isRunning || !isConnected) {
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

    // Thread-safe state read for visibility decision
    bool isVisible;
    bool isRegistered;
    {
        QMutexLocker locker(&m_stateMutex);
        isVisible = m_videoVisible;
        isRegistered = m_containerRegistered;
    }

    if (active) {
        setConnected(true);
        emit projectionStarted();

        // Show video widget if screen is visible
        if (isVisible && isRegistered && m_videoWidget) {
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
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_errorMessage != msg) {
            m_errorMessage = msg;
            changed = true;
        }
    }

    if (changed) {
        emit errorChanged();
        qWarning() << "[OpenAutoEmbedded] Error:" << msg;
    }
}

void OpenAutoEmbedded::setConnected(bool connected)
{
    QString currentPhoneName;
    bool changed = false;

    {
        QMutexLocker locker(&m_stateMutex);
        if (m_connected != connected) {
            m_connected = connected;
            changed = true;

            if (!connected) {
                m_phoneName.clear();
            }
            currentPhoneName = m_phoneName;
        }
    }

    if (changed) {
        emit connectedChanged();

        if (connected) {
            qInfo() << "[OpenAutoEmbedded] Phone connected";
            emit phoneConnected(currentPhoneName);
        } else {
            emit phoneNameChanged();
            emit phoneDisconnected();
            qInfo() << "[OpenAutoEmbedded] Phone disconnected";
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// ISO 26262 DEFENSIVE PROGRAMMING HELPERS
// ═══════════════════════════════════════════════════════════════

int OpenAutoEmbedded::safeCoordinate(qreal value, int minVal, int maxVal) const
{
    // Handle non-finite values (NaN, Inf)
    if (!std::isfinite(value)) {
        qWarning() << "[OpenAutoEmbedded] Non-finite coordinate value detected, using minimum";
        return minVal;
    }

    // Clamp to valid range
    if (value < static_cast<qreal>(minVal)) {
        return minVal;
    }
    if (value > static_cast<qreal>(maxVal)) {
        return maxVal;
    }

    // Safe conversion with rounding
    return static_cast<int>(std::round(value));
}

bool OpenAutoEmbedded::validateGeometry(int x, int y, int w, int h) const
{
    // ISO 26262: Comprehensive geometry validation

    // Check for negative coordinates (invalid for screen position)
    if (x < 0 || y < 0) {
        qDebug() << "[OpenAutoEmbedded] Negative coordinate detected";
        return false;
    }

    // Check for excessive coordinates (overflow protection)
    if (x > MAX_COORDINATE || y > MAX_COORDINATE) {
        qDebug() << "[OpenAutoEmbedded] Coordinate exceeds maximum";
        return false;
    }

    // Check for invalid dimensions
    if (w < MIN_DIMENSION || h < MIN_DIMENSION) {
        qDebug() << "[OpenAutoEmbedded] Dimension below minimum";
        return false;
    }

    // Check for excessive dimensions
    if (w > MAX_COORDINATE || h > MAX_COORDINATE) {
        qDebug() << "[OpenAutoEmbedded] Dimension exceeds maximum";
        return false;
    }

    // Check for overflow in position + dimension calculation
    if (x > MAX_COORDINATE - w || y > MAX_COORDINATE - h) {
        qDebug() << "[OpenAutoEmbedded] Position + dimension would overflow";
        return false;
    }

    return true;
}

} // namespace speeduino

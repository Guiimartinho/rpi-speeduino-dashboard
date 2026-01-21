#include "hmi/openauto_embedded.hpp"
#include "hmi/qml_video_output.hpp"
#include <QDebug>
#include <QApplication>
#include <QTimer>
#include <QMouseEvent>

// libusb
#include <libusb-1.0/libusb.h>

// boost
#include <boost/asio.hpp>

// std
#include <future>

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

    // FIX #10: Exception loop prevention with exponential backoff (ISO 26262)
    int consecutiveExceptions = 0;
    int currentRetryDelayMs = INITIAL_RETRY_DELAY_MS;

    while (m_running.load(std::memory_order_acquire)) {
        try {
            // FIX #4: Call reset() BEFORE run() on each iteration
            // ISO 26262: io_service must be reset before run() can process new handlers
            // After run() returns (no more work), reset() must be called before next run()
            // After exception, reset() is required before run() can resume
            m_ioService.reset();

            m_ioService.run();

            // Successful run resets the exception counter
            consecutiveExceptions = 0;
            currentRetryDelayMs = INITIAL_RETRY_DELAY_MS;

            // NOTE: reset() moved to start of loop - it's needed BEFORE run(), not after
        } catch (const std::exception& e) {
            consecutiveExceptions++;
            qWarning() << "[OpenAutoEmbedded] IO service exception (" << consecutiveExceptions
                       << "/" << MAX_CONSECUTIVE_EXCEPTIONS << "):" << e.what();

            // FIX #10: Check if we've exceeded maximum consecutive exceptions
            if (consecutiveExceptions >= MAX_CONSECUTIVE_EXCEPTIONS) {
                QString fatalMsg = QString("IO service fatal error: %1 consecutive exceptions. "
                                          "Last error: %2")
                                          .arg(consecutiveExceptions)
                                          .arg(e.what());
                qCritical() << "[OpenAutoEmbedded]" << fatalMsg;
                emit fatalError(fatalMsg);
                m_running.store(false, std::memory_order_release);
                break;
            }

            // Exponential backoff delay before retry
            qDebug() << "[OpenAutoEmbedded] Retry delay:" << currentRetryDelayMs << "ms";
            QThread::msleep(static_cast<unsigned long>(currentRetryDelayMs));

            // Double delay for next exception (capped at max)
            currentRetryDelayMs = qMin(currentRetryDelayMs * 2, MAX_RETRY_DELAY_MS);
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
    // QMLVideoOutput will be created in initializeOpenauto() when we have the configuration
    qInfo() << "[OpenAutoEmbedded] Created (QML-native video output mode)";
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
    stop();
}

// ═══════════════════════════════════════════════════════════════
// VIDEO OUTPUT (QML-native approach)
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::setVideoVisible(bool visible)
{
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_videoVisible == visible) {
            return;
        }
        m_videoVisible = visible;
    }

    qInfo() << "[OpenAutoEmbedded] Video visibility:" << visible;

    // QML VideoOutput handles visibility automatically through QML bindings
    // We just track the state here for consistency

    emit videoVisibleChanged();
}

// ═══════════════════════════════════════════════════════════════
// CONTROL METHODS
// ═══════════════════════════════════════════════════════════════

bool OpenAutoEmbedded::start()
{
    // FIX: Thread-safe running check - set m_running BEFORE initialization
    // to prevent race condition where two start() calls can both pass the check
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_running) {
            qWarning() << "[OpenAutoEmbedded] Already running";
            return true;
        }
        // CRITICAL: Mark as running BEFORE initialization to prevent double-init
        m_running = true;
    }

    qInfo() << "[OpenAutoEmbedded] Starting...";

    // Initialize libusb
    if (!initializeLibusb()) {
        // Reset running flag on failure
        QMutexLocker locker(&m_stateMutex);
        m_running = false;
        return false;
    }

    // Initialize openauto components
    if (!initializeOpenauto()) {
        cleanupLibusb();
        // Reset running flag on failure
        QMutexLocker locker(&m_stateMutex);
        m_running = false;
        return false;
    }

    // Clear any previous error message on successful start
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_errorMessage.isEmpty()) {
            m_errorMessage.clear();
        }
    }

    emit errorChanged();
    emit runningChanged();
    emit started();

    qInfo() << "[OpenAutoEmbedded] Started successfully, scheduling device wait...";

    // FIX: Capture m_app as shared_ptr snapshot to prevent use-after-free
    // if stop() is called before the lambda executes
    std::shared_ptr<openauto::App> appSnapshot = m_app;
    if (appSnapshot) {
        // FIX: Use QPointer to safely handle object destruction
        QPointer<OpenAutoEmbedded> weakThis(this);
        QTimer::singleShot(0, this, [weakThis, appSnapshot]() {
            if (!weakThis) {
                qWarning() << "[OpenAutoEmbedded] Object destroyed before device wait";
                return;
            }
            // Safety: Re-check running state after deferral
            // FIX: Use m_waitingForDevice to prevent concurrent waitForDevice() calls
            // aasdk is NOT thread-safe for concurrent device enumeration
            bool canWait = false;
            {
                QMutexLocker locker(&weakThis->m_stateMutex);
                if (weakThis->m_running && !weakThis->m_waitingForDevice) {
                    weakThis->m_waitingForDevice = true;  // Mark as waiting
                    canWait = true;
                }
            }
            if (canWait && appSnapshot) {
                qInfo() << "[OpenAutoEmbedded] Starting device wait (deferred)";
                appSnapshot->waitForDevice(true);

                // FIX: Reset m_waitingForDevice after 10s timeout to allow future retries
                // If waitForDevice() fails silently (e.g., no devices found), we need to
                // allow retryDeviceDetection() to be called when phones are connected later.
                // Without this timeout, m_waitingForDevice stays true forever blocking all retries.
                QTimer::singleShot(10000, [weakThis]() {
                    if (weakThis) {
                        QMutexLocker locker(&weakThis->m_stateMutex);
                        if (!weakThis->m_connected) {
                            weakThis->m_waitingForDevice = false;
                            qInfo() << "[OpenAutoEmbedded] Initial enumeration timeout - allowing retries";
                        }
                    }
                });
            }
        });
    }

    // NOTE: Removed early retry mechanism - it was causing race conditions with aasdk's
    // Promise system. The initial waitForDevice() should handle device detection.
    // If a device was connected before start(), waitForDevice() will find it.
    // The m_pendingRetry flag is no longer used here.
    {
        QMutexLocker locker(&m_stateMutex);
        m_pendingRetry = false;  // Clear any pending retry - initial enumeration will handle it
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

    // Stop QML video output
    if (m_qmlVideoOutput) {
        m_qmlVideoOutput->stop();
    }

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
        m_waitingForDevice = false;  // FIX: Reset waiting flag on stop
        // NOTE: Don't reset m_restarting here - it's managed by restart() timing
    }

    setConnected(false);
    emit runningChanged();
    emit stopped();

    qInfo() << "[OpenAutoEmbedded] Stopped";
}

// MISRA 15.6 FIX: Helper function for restart attempt to reduce nesting depth
void OpenAutoEmbedded::tryRestartAttempt(int attemptNumber, int delayMs)
{
    qInfo() << "[OpenAutoEmbedded] Scheduling restart attempt" << attemptNumber << "in" << delayMs << "ms";
    QPointer<OpenAutoEmbedded> weakThis(this);
    QTimer::singleShot(delayMs, this, [weakThis, attemptNumber]() {
        qInfo() << "[OpenAutoEmbedded] Restart timer fired, attempt" << attemptNumber;
        if (!weakThis) {
            qWarning() << "[OpenAutoEmbedded] Object destroyed before restart could complete";
            return;
        }

        qInfo() << "[OpenAutoEmbedded] Calling start() for restart attempt" << attemptNumber;
        if (weakThis->start()) {
            // Success - clear restarting flag
            QMutexLocker locker(&weakThis->m_stateMutex);
            weakThis->m_restarting = false;
            return;
        }

        // Start failed - handle based on attempt number
        constexpr int MAX_RESTART_ATTEMPTS = 3;
        if (attemptNumber >= MAX_RESTART_ATTEMPTS) {
            qCritical() << "[OpenAutoEmbedded] Restart failed after" << MAX_RESTART_ATTEMPTS << "attempts";
            QMutexLocker locker(&weakThis->m_stateMutex);
            weakThis->m_restarting = false;
            return;
        }

        // Schedule next attempt with increasing delay
        const int nextDelay = (attemptNumber == 1) ? 5000 : 8000;
        qWarning() << "[OpenAutoEmbedded] Restart attempt" << attemptNumber << "failed, retrying in" << nextDelay << "ms";
        weakThis->tryRestartAttempt(attemptNumber + 1, nextDelay);
    });
}

void OpenAutoEmbedded::restart()
{
    // FIX: Debounce restart() to prevent multiple simultaneous restarts
    // Each click on Restart button triggers this, causing "Address already in use" errors
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_restarting) {
            qWarning() << "[OpenAutoEmbedded] Restart already in progress, ignoring request";
            return;
        }
        m_restarting = true;
    }

    qInfo() << "[OpenAutoEmbedded] Restarting...";

    // PHASE 2: Request graceful disconnect before stopping
    // This sends disconnect to phone and waits for clean disconnection
    // Prevents "AaSdk error code: 30" on restart by allowing phone to cleanly exit
    constexpr int GRACEFUL_DISCONNECT_TIMEOUT_MS = 2000;
    bool cleanDisconnect = requestGracefulDisconnect(GRACEFUL_DISCONNECT_TIMEOUT_MS);
    if (!cleanDisconnect) {
        qWarning() << "[OpenAutoEmbedded] Graceful disconnect timed out, proceeding with forced stop";
    }

    stop();

    // PHASE 3: Use shorter delay since graceful disconnect already handled cleanup
    // Reduced from 5s to 2s since phone has already been notified
    constexpr int RESTART_DELAY_MS = 2000;
    // MISRA 15.6 FIX: Extracted to helper function to reduce nesting depth
    tryRestartAttempt(1, RESTART_DELAY_MS);
}

// ═══════════════════════════════════════════════════════════════
// INPUT HANDLING
// ═══════════════════════════════════════════════════════════════

// Touch/input coordinate limits
static constexpr int MAX_COORDINATE = 10000;

void OpenAutoEmbedded::sendTouch(int x, int y, int action)
{
    // Thread-safe state check AND widget pointer capture
    QWidget* inputWidgetPtr = nullptr;
    bool isRunning;
    bool isConnected;
    int widgetWidth = 0;
    int widgetHeight = 0;

    {
        QMutexLocker locker(&m_stateMutex);
        isRunning = m_running;
        isConnected = m_connected;

        if (m_inputWidget) {
            inputWidgetPtr = m_inputWidget.get();
            widgetWidth = m_inputWidget->width();
            widgetHeight = m_inputWidget->height();
        }
    }

    if (!isRunning || !isConnected) {
        return;
    }

    if (!inputWidgetPtr) {
        qWarning() << "[OpenAutoEmbedded] Input widget is null, cannot send touch";
        return;
    }

    // Validate touch coordinates
    if (x < 0 || y < 0 || x > MAX_COORDINATE || y > MAX_COORDINATE) {
        qWarning() << "[OpenAutoEmbedded] Touch coordinates out of bounds:" << x << y;
        return;
    }

    // Clamp coordinates to widget dimensions
    const int maxX = widgetWidth > 0 ? widgetWidth : m_width;
    const int maxY = widgetHeight > 0 ? widgetHeight : m_height;

    // DEBUG: Log coordinate transformation
    if (maxX == 0 || maxY == 0) {
        qWarning() << "[OpenAutoEmbedded] Touch clamp bounds are zero! widgetWidth:" << widgetWidth
                   << "widgetHeight:" << widgetHeight << "m_width:" << m_width << "m_height:" << m_height;
    }

    const int safeX = qBound(0, x, maxX > 0 ? maxX : 10000);
    const int safeY = qBound(0, y, maxY > 0 ? maxY : 10000);

    QPointF localPos(safeX, safeY);
    QPointF globalPos = localPos;

    QEvent::Type eventType;
    Qt::MouseButton button = Qt::LeftButton;
    Qt::MouseButtons buttons;

    // Thread-safe touch state access
    {
        QMutexLocker locker(&m_stateMutex);

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
                button = Qt::NoButton;
                break;
            default:
                qWarning() << "[OpenAutoEmbedded] Unknown touch action:" << action;
                return;
        }
    }

    // Create and post the mouse event to the input widget
    // InputDevice event filter intercepts and forwards to Android Auto
    QMouseEvent* mouseEvent = new QMouseEvent(
        eventType,
        localPos,
        globalPos,
        button,
        buttons,
        Qt::NoModifier
    );

    QCoreApplication::postEvent(inputWidgetPtr, mouseEvent);

    qDebug() << "[OpenAutoEmbedded] Touch event posted:" << safeX << safeY << "action:" << action
             << "(input:" << x << y << "bounds:" << maxX << maxY << ")";
}

void OpenAutoEmbedded::sendKey(int keyCode, bool pressed)
{
    // Thread-safe state check AND widget pointer capture
    QWidget* inputWidgetPtr = nullptr;
    bool isRunning;
    bool isConnected;
    {
        QMutexLocker locker(&m_stateMutex);
        isRunning = m_running;
        isConnected = m_connected;

        if (m_inputWidget) {
            inputWidgetPtr = m_inputWidget.get();
        }
    }

    if (!isRunning || !isConnected) {
        return;
    }

    if (!inputWidgetPtr) {
        qWarning() << "[OpenAutoEmbedded] Input widget is null, cannot send key";
        return;
    }

    QEvent::Type eventType = pressed ? QEvent::KeyPress : QEvent::KeyRelease;

    QKeyEvent* keyEvent = new QKeyEvent(
        eventType,
        keyCode,
        Qt::NoModifier
    );

    QCoreApplication::postEvent(inputWidgetPtr, keyEvent);

    qDebug() << "[OpenAutoEmbedded] Key event posted:" << keyCode << "pressed:" << pressed;
}

// ═══════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::setResolution(int width, int height)
{
    // MEDIUM FIX: Thread-safe resolution update
    {
        QMutexLocker locker(&m_stateMutex);
        m_width = width;
        m_height = height;
    }

    // Update input widget size (for coordinate mapping)
    if (m_inputWidget) {
        m_inputWidget->resize(width, height);
    }

    // Notify serviceFactory of resolution change
    if (m_serviceFactory) {
        m_serviceFactory->resize();
    }

    qInfo() << "[OpenAutoEmbedded] Resolution set to" << width << "x" << height;
}

void OpenAutoEmbedded::setNightMode(bool nightMode)
{
    // MEDIUM FIX: Thread-safe night mode update
    {
        QMutexLocker locker(&m_stateMutex);
        m_nightMode = nightMode;
    }

    if (m_serviceFactory) {
        m_serviceFactory->setNightMode(nightMode);
    }

    qInfo() << "[OpenAutoEmbedded] Night mode:" << nightMode;
}

void OpenAutoEmbedded::retryDeviceDetection()
{
    // Thread-safe state check with snapshot of m_app
    bool isRunning;
    bool isConnected;
    bool isWaiting;
    std::shared_ptr<openauto::App> appSnapshot;
    {
        QMutexLocker locker(&m_stateMutex);
        isRunning = m_running;
        isConnected = m_connected;
        isWaiting = m_waitingForDevice;
        appSnapshot = m_app;  // Capture snapshot while locked
    }

    // FIX: If not running yet, queue the retry for when start() completes
    // This handles the case where USB detection happens before OpenAuto is initialized
    if (!isRunning) {
        {
            QMutexLocker locker(&m_stateMutex);
            m_pendingRetry = true;
        }
        qInfo() << "[OpenAutoEmbedded] retryDeviceDetection: Not running yet, queued for after start()";
        return;
    }

    if (isConnected) {
        qDebug() << "[OpenAutoEmbedded] retryDeviceDetection: Already connected, ignoring";
        return;
    }

    if (!appSnapshot) {
        qDebug() << "[OpenAutoEmbedded] retryDeviceDetection: m_app not initialized yet, ignoring";
        return;
    }

    // FIX: Prevent rapid retries - aasdk's ConnectedAccessoriesEnumerator doesn't handle
    // concurrent enumerate() calls well (returns OPERATION_IN_PROGRESS error 31).
    // The bug in aasdk is that reset() is not called on some error paths, leaving promise_ set.
    // Workaround: Use longer delay (5s) to give time for previous enumeration to complete/fail.
    if (isWaiting) {
        qInfo() << "[OpenAutoEmbedded] Retry already pending, ignoring duplicate request";
        return;
    }

    qInfo() << "[OpenAutoEmbedded] External USB detection triggered - scheduling enumeration (5s delay)";

    // Mark as waiting immediately to prevent duplicate calls
    {
        QMutexLocker locker(&m_stateMutex);
        m_waitingForDevice = true;
    }

    // FIX: Use safe captures to prevent use-after-free
    // FIX: Use 5 second delay to give aasdk time to complete/fail previous enumeration.
    // The aasdk ConnectedAccessoriesEnumerator has a bug where it doesn't reset state on
    // some error paths (especially USB_LIST_DEVICES error), causing subsequent enumerate()
    // calls to fail with OPERATION_IN_PROGRESS. A longer delay helps avoid this race condition.
    QPointer<OpenAutoEmbedded> weakThis(this);
    QTimer::singleShot(5000, this, [weakThis, appSnapshot]() {
        if (!weakThis) {
            qWarning() << "[OpenAutoEmbedded] Object destroyed before retry";
            return;
        }

        // Check if still running and not connected
        bool canRetry = false;
        {
            QMutexLocker locker(&weakThis->m_stateMutex);
            if (weakThis->m_running && !weakThis->m_connected) {
                canRetry = true;
            } else {
                // Reset waiting flag if we can't retry
                weakThis->m_waitingForDevice = false;
            }
        }

        if (canRetry && appSnapshot) {
            qInfo() << "[OpenAutoEmbedded] Triggering USB device enumeration...";
            // NOTE: waitForDevice(true) calls both USBHub::start() and enumerateDevices()
            // USBHub::start() aborts previous promise (error 30) which is OK.
            // enumerateDevices() may fail with error 31 if previous enumeration didn't reset.
            appSnapshot->waitForDevice(true);

            // Reset waiting flag after 10s timeout to allow future retries
            // This handles the case where enumeration fails silently
            QTimer::singleShot(10000, [weakThis]() {
                if (weakThis) {
                    QMutexLocker locker(&weakThis->m_stateMutex);
                    if (!weakThis->m_connected) {
                        weakThis->m_waitingForDevice = false;
                        qInfo() << "[OpenAutoEmbedded] Enumeration timeout - allowing new retries";
                    }
                }
            });
        } else {
            // Reset waiting flag if we didn't retry
            QMutexLocker locker(&weakThis->m_stateMutex);
            weakThis->m_waitingForDevice = false;
        }
    });
}

// ═══════════════════════════════════════════════════════════════
// PROJECTION LIFECYCLE
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::onProjectionActive(bool active)
{
    qInfo() << "[OpenAutoEmbedded] Projection active:" << active;

    {
        QMutexLocker locker(&m_stateMutex);
        if (active) {
            m_waitingForDevice = false;
        }
    }

    if (active) {
        setConnected(true);
        emit projectionStarted();

        // QMLVideoOutput will start playing automatically when data arrives
        qInfo() << "[OpenAutoEmbedded] Projection started (QML-native video)";
    } else {
        setConnected(false);
        emit projectionStopped();

        // Stop QML video output
        if (m_qmlVideoOutput) {
            m_qmlVideoOutput->stop();
        }
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

// ═══════════════════════════════════════════════════════════════
// PHASE 1: USB WORKER SYNCHRONIZATION
// ═══════════════════════════════════════════════════════════════

void OpenAutoEmbedded::stopUsbWorkersSync()
{
    qInfo() << "[OpenAutoEmbedded] Stopping USB worker threads with synchronization...";

    // Signal all workers to stop
    m_usbWorkersRunning.store(false, std::memory_order_release);

    // CRITICAL: Interrupt any threads blocked in libusb_handle_events_*()
    // Without this, workers will remain blocked until their 1-second timeout expires
    if (m_usbContext) {
        libusb_interrupt_event_handler(m_usbContext);
    }

    // Wait for all workers to exit using condition_variable with timeout
    // This prevents race condition where libusb_exit() is called while workers
    // are still inside libusb_handle_events_timeout_completed()
    {
        std::unique_lock<std::mutex> lock(m_usbWorkerMutex);
        constexpr int USB_WORKER_TIMEOUT_MS = 3000;  // 3 second timeout

        bool allExited = m_usbWorkerCV.wait_for(lock,
            std::chrono::milliseconds(USB_WORKER_TIMEOUT_MS),
            [this]() { return m_usbWorkersActive.load() == 0; });

        if (!allExited) {
            qWarning() << "[OpenAutoEmbedded] USB workers did not exit in time,"
                       << m_usbWorkersActive.load() << "still active";
        } else {
            qInfo() << "[OpenAutoEmbedded] All USB workers signaled exit";
        }
    }

    // Now join the threads (should return immediately since they've exited)
    for (auto& thread : m_usbWorkerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_usbWorkerThreads.clear();

    qInfo() << "[OpenAutoEmbedded] USB worker threads stopped and joined";
}

// ═══════════════════════════════════════════════════════════════
// PHASE 2: GRACEFUL DISCONNECT
// ═══════════════════════════════════════════════════════════════

bool OpenAutoEmbedded::requestGracefulDisconnect(int timeoutMs)
{
    // Check if we're connected
    bool wasConnected = false;
    {
        QMutexLocker locker(&m_stateMutex);
        wasConnected = m_connected;
    }

    if (!wasConnected) {
        qInfo() << "[OpenAutoEmbedded] Not connected, no graceful disconnect needed";
        return true;
    }

    qInfo() << "[OpenAutoEmbedded] Requesting graceful disconnect (timeout:" << timeoutMs << "ms)";

    // Stop the app - this sends disconnect to the phone
    if (m_app) {
        m_app->stop();
    }

    // Wait for disconnect with polling (condition_variable not available for m_connected)
    constexpr int POLL_INTERVAL_MS = 100;
    int elapsedMs = 0;

    while (elapsedMs < timeoutMs) {
        {
            QMutexLocker locker(&m_stateMutex);
            if (!m_connected) {
                qInfo() << "[OpenAutoEmbedded] Phone disconnected cleanly after" << elapsedMs << "ms";
                return true;
            }
        }

        QThread::msleep(POLL_INTERVAL_MS);
        elapsedMs += POLL_INTERVAL_MS;

        // Process Qt events to allow callbacks to fire
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    qWarning() << "[OpenAutoEmbedded] Graceful disconnect timeout - forcing cleanup";
    return false;
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

        // Create accessory mode query factories (MUST be member variables - lifetime!)
        m_queryFactory = std::make_unique<aasdk::usb::AccessoryModeQueryFactory>(
            *m_usbWrapper, *m_ioService);
        m_queryChainFactory = std::make_unique<aasdk::usb::AccessoryModeQueryChainFactory>(
            *m_usbWrapper, *m_ioService, *m_queryFactory);

        // Create hidden input widget for touch event forwarding
        // The widget needs valid geometry to avoid crash in ServiceFactory::mapActiveAreaToGlobal
        // We set explicit size BEFORE showing/hiding to ensure valid geometry
        m_inputWidget = std::make_unique<QWidget>();
        m_inputWidget->setObjectName("OpenAutoInputWidget");
        m_inputWidget->setGeometry(0, 0, m_width, m_height);  // Set valid geometry FIRST
        m_inputWidget->setFixedSize(m_width, m_height);       // Prevent resize
        m_inputWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        m_inputWidget->setAttribute(Qt::WA_OpaquePaintEvent, false);
        m_inputWidget->setFocusPolicy(Qt::StrongFocus);
        // Hide the widget - we only need it for event forwarding, not display
        // The video is displayed via QML VideoOutput
        m_inputWidget->hide();
        qInfo() << "[OpenAutoEmbedded] Input widget created:" << m_width << "x" << m_height;

        // Create QML-native video output
        // This renders H.264 video to a QVideoSink that QML can display
        // CRITICAL FIX: Do NOT pass 'this' as QObject parent when using shared_ptr!
        // QObject parent ownership conflicts with shared_ptr ownership, causing
        // premature destruction when Qt's object tree is modified.
        m_qmlVideoOutput = std::make_shared<QMLVideoOutput>(m_configuration, nullptr);

        // Notify QML that qmlVideoOutput is now available
        emit qmlVideoOutputChanged();

        // Projection active callback
        QPointer<OpenAutoEmbedded> weakThis(this);
        auto activeCallback = [weakThis](bool active) {
            if (weakThis) {
                QMetaObject::invokeMethod(weakThis.data(), "onProjectionActive", Qt::QueuedConnection,
                                          Q_ARG(bool, active));
            }
        };

        // CRITICAL FIX: Connect QMLVideoOutput playback signals to projection callback
        // ServiceFactory doesn't connect activeCallback for custom video outputs,
        // so we must do it here manually
        connect(m_qmlVideoOutput.get(), &QMLVideoOutput::playbackStarted, this, [activeCallback]() {
            qInfo() << "[OpenAutoEmbedded] QMLVideoOutput playback started - triggering projection active";
            activeCallback(true);
        });
        // NOTE: Do NOT connect playbackStopped to activeCallback(false) here!
        // onProjectionActive(false) already calls m_qmlVideoOutput->stop(), which emits playbackStopped.
        // Connecting playbackStopped → activeCallback(false) creates an infinite loop:
        // playbackStopped → activeCallback(false) → onProjectionActive(false) → stop() → playbackStopped

        // Create ServiceFactory with custom video output and input widget
        // Video goes to QMLVideoOutput, input events go to m_inputWidget
        // Input widget provides geometry for mapActiveAreaToGlobal() and receives touch events
        m_serviceFactory = std::make_unique<openauto::service::ServiceFactory>(
            *m_ioService,
            m_configuration,
            m_qmlVideoOutput,           // QML-native video output
            m_inputWidget.get(),        // Hidden widget for touch input forwarding
            activeCallback,
            m_nightMode
        );

        // Create AndroidAutoEntityFactory
        m_androidAutoEntityFactory = std::make_unique<openauto::service::AndroidAutoEntityFactory>(
            *m_ioService,
            m_configuration,
            *m_serviceFactory
        );

        // Create USB hub (uses reference to member m_queryChainFactory)
        m_usbHub = std::make_shared<aasdk::usb::USBHub>(
            *m_usbWrapper,
            *m_ioService,
            *m_queryChainFactory
        );

        // Create connected accessories enumerator (uses reference to member m_queryChainFactory)
        m_connectedAccessoriesEnumerator = std::make_shared<aasdk::usb::ConnectedAccessoriesEnumerator>(
            *m_usbWrapper,
            *m_ioService,
            *m_queryChainFactory
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
        // FIX #2: Worker lifecycle managed explicitly by unique_ptr
        // DO NOT use deleteLater - it causes double-free with unique_ptr ownership
        m_ioThread = std::make_unique<QThread>();
        m_ioWorker = std::make_unique<OpenAutoIOWorker>(*m_ioService);
        m_ioWorker->moveToThread(m_ioThread.get());

        connect(m_ioThread.get(), &QThread::started, m_ioWorker.get(), &OpenAutoIOWorker::run);
        // NOTE: Removed deleteLater connection - worker is deleted in cleanupOpenauto()

        // FIX #10: Connect fatal error signal to handle IO thread exception loop
        connect(m_ioWorker.get(), &OpenAutoIOWorker::fatalError, this, [this](const QString& message) {
            setError(message);
            // Schedule stop on main thread to avoid cross-thread issues
            QMetaObject::invokeMethod(this, "stop", Qt::QueuedConnection);
        });

        m_ioThread->start();

        // CRITICAL FIX: Start USB worker threads to process libusb async transfers!
        // Without these threads calling libusb_handle_events_timeout_completed(),
        // USB control transfers (AOA protocol negotiation) will never complete.
        // This was the root cause of phones not switching to AOA mode.
        //
        // PHASE 1 FIX: USB workers now signal when they exit using condition_variable
        // This allows cleanupOpenauto() to wait for all workers to finish before libusb_exit()
        m_usbWorkersRunning.store(true);
        m_usbWorkersActive.store(0);
        constexpr int NUM_USB_WORKERS = 4;
        for (int i = 0; i < NUM_USB_WORKERS; ++i) {
            m_usbWorkerThreads.emplace_back([this]() {
                // Increment active count on entry
                m_usbWorkersActive.fetch_add(1);

                timeval libusbEventTimeout{1, 0};  // 1 second timeout
                while (m_usbWorkersRunning.load(std::memory_order_acquire) &&
                       m_ioService && !m_ioService->stopped()) {
                    // Check context is still valid before using it
                    if (m_usbContext) {
                        libusb_handle_events_timeout_completed(m_usbContext, &libusbEventTimeout, nullptr);
                    }
                }

                // Decrement active count and signal on exit
                int remaining = m_usbWorkersActive.fetch_sub(1) - 1;
                if (remaining == 0) {
                    // Last worker exiting - notify waiting cleanup
                    std::lock_guard<std::mutex> lock(m_usbWorkerMutex);
                    m_usbWorkerCV.notify_all();
                }
            });
        }
        qInfo() << "[OpenAutoEmbedded] Started" << NUM_USB_WORKERS << "USB worker threads";

        qInfo() << "[OpenAutoEmbedded] OpenAuto components initialized";
        return true;

    } catch (const std::exception& e) {
        setError(QString("Failed to initialize OpenAuto: %1").arg(e.what()));
        return false;
    }
}

void OpenAutoEmbedded::cleanupOpenauto()
{
    qInfo() << "[OpenAutoEmbedded] cleanupOpenauto() starting...";

    // PHASE 1: Stop USB worker threads with proper synchronization
    // Uses condition_variable to wait for all workers to exit before continuing
    // This prevents race condition where libusb_exit() is called while workers
    // are still inside libusb_handle_events_timeout_completed()
    stopUsbWorkersSync();

    // Stop IO worker (signals worker to exit its run loop)
    // NOTE: Keep io_service running - serviceFactory components need it for shutdown callbacks
    if (m_ioWorker) {
        qInfo() << "[OpenAutoEmbedded] Stopping IO worker...";
        m_ioWorker->stop();
    }

    // Clear app and components in reverse order of creation
    qInfo() << "[OpenAutoEmbedded] Resetting m_app...";
    m_app.reset();
    qInfo() << "[OpenAutoEmbedded] m_app reset complete";

    qInfo() << "[OpenAutoEmbedded] Resetting enumerator and hub...";
    m_connectedAccessoriesEnumerator.reset();
    m_usbHub.reset();

    // Reset query factories AFTER USBHub/Enumerator (they hold references)
    qInfo() << "[OpenAutoEmbedded] Resetting query factories...";
    m_queryChainFactory.reset();
    m_queryFactory.reset();

    qInfo() << "[OpenAutoEmbedded] Resetting androidAutoEntityFactory...";
    m_androidAutoEntityFactory.reset();

    // CRITICAL: Reset QML video output BEFORE serviceFactory
    // ServiceFactory holds a shared_ptr to m_qmlVideoOutput. If we reset serviceFactory
    // while video output still has active GStreamer pipelines, the destructor may crash.
    // Reset video output first to ensure clean GStreamer shutdown.
    if (m_qmlVideoOutput) {
        qInfo() << "[OpenAutoEmbedded] Stopping QML video output...";
        m_qmlVideoOutput->stop();
        qInfo() << "[OpenAutoEmbedded] Disconnecting QML video output signals...";
        disconnect(m_qmlVideoOutput.get(), nullptr, this, nullptr);
        qInfo() << "[OpenAutoEmbedded] Resetting QML video output BEFORE serviceFactory...";
        // Note: ServiceFactory also holds a shared_ptr, so this won't destroy it yet
        // but it will release our reference.
        // DO NOT emit qmlVideoOutputChanged() here - it triggers QML to try reconnecting
        // during cleanup, which can cause race conditions and crashes. QML will get a
        // new signal when start() creates a fresh video output.
        m_qmlVideoOutput.reset();
        qInfo() << "[OpenAutoEmbedded] QML video output reset";
    }

    // Stop io_service BEFORE serviceFactory reset
    // This forces async operations to be cancelled, preventing deadlock in destructors
    qInfo() << "[OpenAutoEmbedded] Releasing work guard...";
    m_ioWork.reset();

    if (m_ioService) {
        qInfo() << "[OpenAutoEmbedded] Stopping IO service...";
        m_ioService->stop();
    }

    // Stop IO thread and wait for it to finish
    if (m_ioThread && m_ioThread->isRunning()) {
        qInfo() << "[OpenAutoEmbedded] Stopping IO thread...";
        m_ioThread->quit();
        if (!m_ioThread->wait(3000)) {
            qWarning() << "[OpenAutoEmbedded] IO thread did not stop in time, terminating";
            m_ioThread->terminate();
            m_ioThread->wait();
        }
    }
    qInfo() << "[OpenAutoEmbedded] IO thread stopped";

    qInfo() << "[OpenAutoEmbedded] Resetting serviceFactory...";
    // ServiceFactory destructor can block if channels have pending async operations
    // Use a future with timeout to prevent indefinite hang
    {
        auto serviceFactoryPtr = std::move(m_serviceFactory);
        auto future = std::async(std::launch::async, [ptr = std::move(serviceFactoryPtr)]() mutable {
            ptr.reset();
        });

        constexpr int SERVICE_FACTORY_TIMEOUT_MS = 5000;
        auto status = future.wait_for(std::chrono::milliseconds(SERVICE_FACTORY_TIMEOUT_MS));
        if (status == std::future_status::timeout) {
            qWarning() << "[OpenAutoEmbedded] serviceFactory reset timed out after" << SERVICE_FACTORY_TIMEOUT_MS << "ms";
            // Detach the future - destructor will complete eventually
            // This is a controlled leak to prevent app hang
        } else {
            qInfo() << "[OpenAutoEmbedded] serviceFactory reset complete";
        }
    }

    // Reset hidden input widget
    qInfo() << "[OpenAutoEmbedded] Resetting input widget...";
    m_inputWidget.reset();

    qInfo() << "[OpenAutoEmbedded] Resetting wrappers and configuration...";
    m_tcpWrapper.reset();
    m_usbWrapper.reset();
    m_configuration.reset();

    // Worker must be deleted AFTER thread stops but BEFORE io_service is destroyed
    qInfo() << "[OpenAutoEmbedded] Resetting IO worker and thread...";
    m_ioWorker.reset();
    m_ioThread.reset();

    // Now safe to reset io_service (no references remain)
    qInfo() << "[OpenAutoEmbedded] Resetting IO service...";
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

} // namespace speeduino

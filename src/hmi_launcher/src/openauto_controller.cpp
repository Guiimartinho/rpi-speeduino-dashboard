#include "hmi/openauto_controller.hpp"

#include <QThread>

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QPointer>
#include <QRegularExpression>

#ifdef Q_OS_LINUX
    #include <unistd.h>  // for getuid()
#endif

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// Process Management Timing Constants
// ISO 26262: Named constants for process lifecycle timing
// ═══════════════════════════════════════════════════════════════════════════════
namespace {
/// Timeout for killing a process in milliseconds
constexpr int PROCESS_KILL_TIMEOUT_MS = 1000;
/// Timeout for starting a process in milliseconds
constexpr int PROCESS_START_TIMEOUT_MS = 5000;
/// Timeout for graceful process termination in milliseconds
constexpr int PROCESS_TERMINATE_TIMEOUT_MS = 3000;
/// Delay before restarting after clean stop in milliseconds
constexpr int RESTART_DELAY_MS = 500;
/// Delay for USB device change debouncing in milliseconds
constexpr int USB_DEBOUNCE_DELAY_MS = 500;
/// Interval for USB device polling in milliseconds
constexpr int USB_CHECK_INTERVAL_MS = 2000;
/// Initial USB check delay on startup in milliseconds
constexpr int INITIAL_USB_CHECK_DELAY_MS = 100;
/// Delay to confirm process stability before reset crash counter in milliseconds
constexpr int PROCESS_STABILITY_CHECK_MS = 5000;
}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════════
// Orphan Process Cleanup - FIX for "Address already in use" crash loop
// ═══════════════════════════════════════════════════════════════════════════════

void OpenAutoController::killOrphanProcesses() {
    // HIGH NOTE: These blocking calls are intentional - they run during startup
    // before the event loop needs responsiveness. This ensures clean state
    // before starting OpenAuto. ISO 26262: Deterministic initialization.
    qInfo() << "[OpenAutoController] Killing orphan autoapp processes...";

#ifdef Q_OS_LINUX
    // Kill any existing autoapp processes
    QProcess killProc;
    killProc.start("pkill", QStringList() << "-9"
                                          << "autoapp");
    killProc.waitForFinished(2000);

    // Also kill by full path in case name matching fails
    killProc.start("pkill", QStringList() << "-9"
                                          << "-f"
                                          << "/usr/local/bin/autoapp");
    killProc.waitForFinished(2000);

    // Wait for system to release resources (sockets, BT profiles)
    QThread::msleep(500);

    qInfo() << "[OpenAutoController] Orphan cleanup complete";
#endif
}

void OpenAutoController::resetBluetoothProfile() {
    qInfo() << "[OpenAutoController] Resetting Bluetooth profile...";

#ifdef Q_OS_LINUX
    // Restart bluetooth service to clear orphan profiles
    // This is a workaround for "UUID already registered" error
    QProcess btProc;

    // Try to unregister the AndroidAuto profile first
    btProc.start("bluetoothctl", QStringList() << "unregister"
                                               << "AndroidAuto");
    btProc.waitForFinished(1000);

    // Alternative: restart bluetooth (more aggressive but reliable)
    // btProc.start("systemctl", QStringList() << "restart" << "bluetooth");
    // btProc.waitForFinished(5000);

    qInfo() << "[OpenAutoController] Bluetooth reset complete";
#endif
}

// Known Android Auto compatible device USB IDs (vendor:product)
// These are common Android phone manufacturers that support AA
const QStringList OpenAutoController::AA_USB_IDS = {
    "18d1:",  // Google
    "04e8:",  // Samsung
    "22b8:",  // Motorola
    "0bb4:",  // HTC
    "2717:",  // Xiaomi
    "12d1:",  // Huawei
    "2a70:",  // OnePlus
    "05c6:",  // Qualcomm (many Android phones)
    "1004:",  // LG
    "0fce:",  // Sony
    "2916:",  // Android (generic)
    "1949:",  // Amazon (Fire tablets with AA)
    "2ae5:",  // Fairphone
    "0e8d:",  // MediaTek
    "19d2:",  // ZTE
    "2b4c:",  // Asus
    "0502:",  // Acer
    "1ebf:",  // Oppo/Realme
};

OpenAutoController::OpenAutoController(QObject* parent)
    : QObject(parent), m_process(std::make_unique<QProcess>(this)),
      m_usbCheckTimer(std::make_unique<QTimer>(this)),
      m_usbWatcher(std::make_unique<QFileSystemWatcher>(this)) {
    // Process connections
    connect(m_process.get(), &QProcess::started, this, &OpenAutoController::onProcessStarted);
    connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &OpenAutoController::onProcessFinished);
    connect(m_process.get(), &QProcess::errorOccurred, this, &OpenAutoController::onProcessError);
    connect(m_process.get(), &QProcess::readyReadStandardOutput, this,
            &OpenAutoController::onReadyReadStdout);
    connect(m_process.get(), &QProcess::readyReadStandardError, this,
            &OpenAutoController::onReadyReadStderr);

    // USB monitoring
    connect(m_usbCheckTimer.get(), &QTimer::timeout, this, &OpenAutoController::checkUsbDevices);
    connect(m_usbWatcher.get(), &QFileSystemWatcher::directoryChanged, this,
            &OpenAutoController::onUsbDeviceChanged);

    // Start USB monitoring
    startUsbMonitoring();
}

OpenAutoController::~OpenAutoController() {
    stop();
    stopUsbMonitoring();
}

// ═══════════════════════════════════════════════════════════════
// THREAD-SAFE PROPERTY GETTERS
// ═══════════════════════════════════════════════════════════════

bool OpenAutoController::isRunning() const {
    QMutexLocker locker(&m_stateMutex);
    return m_running;
}

bool OpenAutoController::isConnected() const {
    QMutexLocker locker(&m_stateMutex);
    return m_connected;
}

QString OpenAutoController::errorMessage() const {
    QMutexLocker locker(&m_stateMutex);
    return m_errorMessage;
}

QString OpenAutoController::connectionType() const {
    QMutexLocker locker(&m_stateMutex);
    return m_connectionType;
}

QString OpenAutoController::phoneName() const {
    QMutexLocker locker(&m_stateMutex);
    return m_phoneName;
}

bool OpenAutoController::autoStart() const {
    QMutexLocker locker(&m_stateMutex);
    return m_autoStart;
}

bool OpenAutoController::wirelessEnabled() const {
    QMutexLocker locker(&m_stateMutex);
    return m_wirelessEnabled;
}

// ═══════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════

void OpenAutoController::setExecutablePath(const QString& path) {
    QMutexLocker locker(&m_stateMutex);
    m_executablePath = path;
    qInfo() << "[OpenAutoController] Executable path set to:" << path;
}

void OpenAutoController::setFullscreen(bool fullscreen) {
    QMutexLocker locker(&m_stateMutex);
    m_fullscreen = fullscreen;
}

void OpenAutoController::setAutoStart(bool autoStart) {
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_autoStart != autoStart) {
            m_autoStart = autoStart;
            changed     = true;
        }
    }
    if (changed) {
        emit autoStartChanged();
        qInfo() << "[OpenAutoController] Auto-start:" << (autoStart ? "enabled" : "disabled");
    }
}

void OpenAutoController::setWirelessEnabled(bool enabled) {
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_wirelessEnabled != enabled) {
            m_wirelessEnabled = enabled;
            changed           = true;
        }
    }
    if (changed) {
        emit wirelessEnabledChanged();
        qInfo() << "[OpenAutoController] Wireless Android Auto:"
                << (enabled ? "enabled" : "disabled");
    }
}

void OpenAutoController::setResolution(int width, int height, int fps) {
    QMutexLocker locker(&m_stateMutex);
    m_videoWidth  = width;
    m_videoHeight = height;
    m_videoFps    = fps;
    qInfo() << "[OpenAutoController] Resolution set to:" << width << "x" << height << "@" << fps
            << "fps";
}

bool OpenAutoController::start() {
    // Thread-safe running check
    QString execPath;
    bool isFullscreen;
    int videoW, videoH;
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_running) {
            qWarning() << "[OpenAutoController] Already running";
            return true;
        }
        execPath     = m_executablePath;
        isFullscreen = m_fullscreen;
        videoW       = m_videoWidth;
        videoH       = m_videoHeight;
    }

    clearError();

    // FIX: Kill orphan processes and reset Bluetooth before starting
    // This prevents "Address already in use" and "UUID already registered" errors
    killOrphanProcesses();
    resetBluetoothProfile();

    // Check if executable exists
    if (!QFile::exists(execPath)) {
        // Try alternative paths
        QStringList altPaths = {"/usr/bin/autoapp", "/usr/local/bin/autoapp",
                                "/opt/openauto/bin/autoapp", "/home/pi/openauto/bin/autoapp",
                                QDir::homePath() + "/openauto/bin/autoapp"};

        bool found = false;
        for (const QString& path : altPaths) {
            if (QFile::exists(path)) {
                {
                    QMutexLocker locker(&m_stateMutex);
                    m_executablePath = path;
                }
                execPath = path;
                found    = true;
                qInfo() << "[OpenAutoController] Found OpenAuto at:" << path;
                break;
            }
        }

        if (!found) {
            setError(QString("OpenAuto executable not found. Searched:\n%1\n%2")
                         .arg(execPath)
                         .arg(altPaths.join("\n")));
            return false;
        }
    }

    // Build environment for OpenAuto
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Set windowed mode if not fullscreen
    if (!isFullscreen) {
        env.insert("OPENAUTO_WINDOWED", "1");
        env.insert("OPENAUTO_WIDTH", QString::number(videoW));
        env.insert("OPENAUTO_HEIGHT", QString::number(videoH));
        qInfo() << "[OpenAutoController] Windowed mode:" << videoW << "x" << videoH;
    }

    // Set Qt platform for Wayland
    if (!env.contains("QT_QPA_PLATFORM")) {
        env.insert("QT_QPA_PLATFORM", "wayland");
    }

    // Ensure XDG_RUNTIME_DIR is set
    // MEDIUM FIX: Use actual UID instead of hardcoded 1000
    if (!env.contains("XDG_RUNTIME_DIR")) {
#ifdef Q_OS_LINUX
        env.insert("XDG_RUNTIME_DIR", QString("/run/user/%1").arg(getuid()));
#else
        env.insert("XDG_RUNTIME_DIR", "/run/user/1000");
#endif
    }

    // Ensure WAYLAND_DISPLAY is set
    if (!env.contains("WAYLAND_DISPLAY")) {
        env.insert("WAYLAND_DISPLAY", "wayland-0");
    }

    qInfo() << "[OpenAutoController] Starting OpenAuto:" << execPath;
    qInfo() << "[OpenAutoController]   Windowed:" << (!isFullscreen ? "yes" : "no");
    qInfo() << "[OpenAutoController]   Resolution:" << videoW << "x" << videoH;

    // Validate process state before starting
    if (!m_process) {
        setError("Process object is null");
        return false;
    }

    if (m_process->state() != QProcess::NotRunning) {
        qWarning() << "[OpenAutoController] Process already in running state, killing first";
        m_process->kill();
        m_process->waitForFinished(PROCESS_KILL_TIMEOUT_MS);
    }

    m_process->setProcessEnvironment(env);
    m_process->start(execPath, QStringList());

    if (!m_process->waitForStarted(PROCESS_START_TIMEOUT_MS)) {
        setError("Failed to start OpenAuto process within timeout");
        return false;
    }

    return true;
}

void OpenAutoController::stop() {
    // Thread-safe running check
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_running) {
            return;
        }
    }

    qInfo() << "[OpenAutoController] Stopping...";

    // Validate process before operations
    if (!m_process) {
        qWarning() << "[OpenAutoController] Process is null in stop()";
        return;
    }

    // Send SIGTERM first for graceful shutdown
    m_process->terminate();

    if (!m_process->waitForFinished(PROCESS_TERMINATE_TIMEOUT_MS)) {
        // Force kill if doesn't respond
        qWarning() << "[OpenAutoController] Not responding to SIGTERM, sending SIGKILL...";
        m_process->kill();
        m_process->waitForFinished(PROCESS_KILL_TIMEOUT_MS);
    }

    setConnected(false);
}

void OpenAutoController::restart() {
    qInfo() << "Restarting OpenAuto...";
    stop();

    // HIGH FIX: Use QPointer to prevent use-after-free if object is destroyed
    QPointer<OpenAutoController> weakThis(this);
    QTimer::singleShot(RESTART_DELAY_MS, this, [weakThis]() {
        if (weakThis) {
            weakThis->start();
        }
    });
}

void OpenAutoController::toggle() {
    // Thread-safe running check
    bool isRunning;
    {
        QMutexLocker locker(&m_stateMutex);
        isRunning = m_running;
    }

    if (isRunning) {
        stop();
    } else {
        start();
    }
}

void OpenAutoController::sendTouch(int x, int y, int type) {
    // Thread-safe state check
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_running || !m_connected) {
            return;
        }
    }

    // ISO 26262: Validate touch type (0=press, 1=release, 2=move)
    if (type < 0 || type > 2) {
        qWarning() << "[OpenAutoController] Invalid touch type:" << type;
        return;
    }

    // ISO 26262: Bounds validation for coordinates
    // Use configured video resolution as max bounds
    int maxX, maxY;
    {
        QMutexLocker locker(&m_stateMutex);
        maxX = m_videoWidth;
        maxY = m_videoHeight;
    }

    // Clamp coordinates to valid range (prevent negative or overflow)
    const int safeX = qBound(0, x, maxX);
    const int safeY = qBound(0, y, maxY);

    // Log if coordinates were clamped (potential issue indicator)
    if (safeX != x || safeY != y) {
        qDebug() << "[OpenAutoController] Touch coordinates clamped:" << x << "," << y << "->"
                 << safeX << "," << safeY;
    }

    // Touch types: 0 = press, 1 = release, 2 = move
    // This would send touch events to the OpenAuto process
    // Implementation depends on how OpenAuto accepts input

    // For now, log the touch events (actual implementation requires
    // either IPC with OpenAuto or using an input device)
    qDebug() << "[OpenAutoController] Touch event:" << safeX << safeY << "type:" << type;

    // TODO: Implement actual touch forwarding via:
    // Option 1: Write to a named pipe that OpenAuto reads
    // Option 2: Use DBus to communicate with OpenAuto
    // Option 3: Use uinput to create virtual touch device
}

void OpenAutoController::scanForDevices() {
    qInfo() << "Scanning for Android Auto devices...";
    checkUsbDevices();
}

// MISRA 15.6 FIX: Helper function to check if USB vendor ID matches Android Auto IDs
bool OpenAutoController::isAndroidAutoVendor(const QString& vendorId) const {
    for (const QString& aaId : AA_USB_IDS) {
        if (vendorId.compare(aaId.left(4), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// MISRA 15.6 FIX: Helper function to read device info from USB device path
QString OpenAutoController::readDeviceInfo(const QString& devicePath) const {
    QString deviceName = "Unknown Android Device";

#ifdef Q_OS_LINUX
    QFile mfgFile(devicePath + "/manufacturer");
    QFile prodFile(devicePath + "/product");

    if (mfgFile.open(QIODevice::ReadOnly)) {
        deviceName = QString::fromUtf8(mfgFile.readAll()).trimmed();
        mfgFile.close();
    }
    if (prodFile.open(QIODevice::ReadOnly)) {
        QString prodName = QString::fromUtf8(prodFile.readAll()).trimmed();
        prodFile.close();
        if (!prodName.isEmpty()) {
            deviceName += " " + prodName;
        }
    }
#else
    Q_UNUSED(devicePath)
#endif

    return deviceName;
}

QStringList OpenAutoController::getConnectedDevices() const {
    QStringList devices;

#ifdef Q_OS_LINUX
    // Read from /sys/bus/usb/devices
    QDir usbDir("/sys/bus/usb/devices");
    QStringList entries = usbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries) {
        QString devicePath = usbDir.filePath(entry);
        QFile vendorFile(devicePath + "/idVendor");

        // MISRA 15.6 FIX: Early continue reduces nesting depth
        if (!vendorFile.open(QIODevice::ReadOnly))
            continue;

        QString vendor = QString::fromUtf8(vendorFile.readAll()).trimmed();
        vendorFile.close();

        // MISRA 15.6 FIX: Early continue instead of if block
        if (!isAndroidAutoVendor(vendor))
            continue;

        devices << readDeviceInfo(devicePath);
    }
#endif

    return devices;
}

// Private slots

void OpenAutoController::onProcessStarted() {
    {
        QMutexLocker locker(&m_stateMutex);
        m_running = true;
    }

    // FIX #9: Record successful start time for crash loop detection
    m_lastSuccessfulStart = std::chrono::steady_clock::now();

    emit runningChanged();
    emit started();
    qInfo() << "[OpenAutoController] Process started (PID:" << m_process->processId() << ")";
    emit showNotification("OpenAuto", "Android Auto is ready. Connect your phone.");

    // FIX #9: Reset crash counter after successful start + stable period
    // Use delayed reset to confirm process is stable (not immediately crashing)
    // HIGH FIX: Use QPointer to prevent use-after-free
    QPointer<OpenAutoController> weakThis(this);
    QTimer::singleShot(PROCESS_STABILITY_CHECK_MS, this, [weakThis]() {
        if (!weakThis)
            return;
        bool isStillRunning;
        {
            QMutexLocker locker(&weakThis->m_stateMutex);
            isStillRunning = weakThis->m_running;
        }
        if (isStillRunning) {
            weakThis->m_crashRestartCount     = 0;
            weakThis->m_currentRestartDelayMs = INITIAL_RESTART_DELAY_MS;
            qInfo() << "[OpenAutoController] Process stable, crash counter reset";
        }
    });
}

void OpenAutoController::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    bool shouldAutoRestart = false;

    {
        QMutexLocker locker(&m_stateMutex);
        m_running         = false;
        shouldAutoRestart = m_autoStart;
    }

    emit runningChanged();
    setConnected(false);

    if (status == QProcess::CrashExit) {
        QString msg = QString("OpenAuto crashed with exit code %1").arg(exitCode);
        setError(msg);
        emit crashed(msg);
        emit showNotification("OpenAuto Error", msg);

        // FIX #9: Crash loop prevention with exponential backoff (ISO 26262)
        if (shouldAutoRestart) {
            m_crashRestartCount++;

            if (m_crashRestartCount > MAX_CRASH_RESTARTS) {
                QString loopMsg = QString(
                                      "OpenAuto crash loop detected (%1 crashes). "
                                      "Automatic restart disabled. Manual intervention required.")
                                      .arg(m_crashRestartCount);
                setError(loopMsg);
                emit showNotification("OpenAuto Error", "Crash loop detected - restart disabled");
                qCritical() << "[OpenAutoController]" << loopMsg;
                // Reset for future manual restart attempts
                m_crashRestartCount     = 0;
                m_currentRestartDelayMs = INITIAL_RESTART_DELAY_MS;
            } else {
                qInfo() << "[OpenAutoController] Auto-restarting after crash"
                        << "(" << m_crashRestartCount << "/" << MAX_CRASH_RESTARTS << ")"
                        << "delay:" << m_currentRestartDelayMs << "ms";

                // Schedule restart with current delay
                // HIGH FIX: Use QPointer to prevent use-after-free
                QPointer<OpenAutoController> weakThis(this);
                QTimer::singleShot(m_currentRestartDelayMs, this, [weakThis]() {
                    if (weakThis)
                        weakThis->start();
                });

                // Exponential backoff for next crash (double delay, capped at max)
                m_currentRestartDelayMs = qMin(m_currentRestartDelayMs * 2, MAX_RESTART_DELAY_MS);
            }
        }
    } else {
        qInfo() << "[OpenAutoController] Stopped normally with exit code" << exitCode;
        // FIX #9: Reset crash counter on clean exit
        m_crashRestartCount     = 0;
        m_currentRestartDelayMs = INITIAL_RESTART_DELAY_MS;
        emit stopped();
    }
}

void OpenAutoController::onProcessError(QProcess::ProcessError error) {
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start OpenAuto - check executable path and permissions";
            break;
        case QProcess::Crashed:
            errorMsg = "OpenAuto process crashed unexpectedly";
            break;
        case QProcess::Timedout:
            errorMsg = "OpenAuto process operation timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Write error communicating with OpenAuto";
            break;
        case QProcess::ReadError:
            errorMsg = "Read error communicating with OpenAuto";
            break;
        default:
            errorMsg = QString("Unknown OpenAuto error (%1)").arg(static_cast<int>(error));
            break;
    }

    setError(errorMsg);
}

void OpenAutoController::onReadyReadStdout() {
    QByteArray data = m_process->readAllStandardOutput();
    QString output  = QString::fromUtf8(data).trimmed();

    if (output.isEmpty())
        return;

    qDebug() << "[OpenAuto]" << output;

    // Parse OpenAuto output for connection status
    // These strings depend on OpenAuto's actual output format
    if (output.contains("Phone connected", Qt::CaseInsensitive) ||
        output.contains("AndroidAuto started", Qt::CaseInsensitive) ||
        output.contains("Projection started", Qt::CaseInsensitive)) {
        setConnected(true);
        // FIX #1: Thread-safe connection type update (race condition fix)
        {
            QMutexLocker locker(&m_stateMutex);
            m_connectionType = "USB";
        }
        emit connectionTypeChanged();
        emit showNotification("Android Auto", "Phone connected successfully!");
    } else if (output.contains("Phone disconnected", Qt::CaseInsensitive) ||
               output.contains("AndroidAuto stopped", Qt::CaseInsensitive) ||
               output.contains("Projection stopped", Qt::CaseInsensitive)) {
        setConnected(false);
    } else if (output.contains("Wireless connection", Qt::CaseInsensitive)) {
        // FIX #1: Thread-safe connection type update (race condition fix)
        {
            QMutexLocker locker(&m_stateMutex);
            m_connectionType = "Wireless";
        }
        emit connectionTypeChanged();
    }
}

void OpenAutoController::onReadyReadStderr() {
    QByteArray data = m_process->readAllStandardError();
    QString output  = QString::fromUtf8(data).trimmed();

    if (!output.isEmpty()) {
        qWarning() << "[OpenAuto ERROR]" << output;
    }
}

void OpenAutoController::checkUsbDevices() {
    bool deviceFound = detectAndroidAutoDevice();

    // Thread-safe state read
    bool isConnected, isAutoStart, isRunning;
    {
        QMutexLocker locker(&m_stateMutex);
        isConnected = m_connected;
        isAutoStart = m_autoStart;
        isRunning   = m_running;
    }

    if (deviceFound && !isConnected && isAutoStart && !isRunning) {
        qInfo() << "[OpenAutoController] Android Auto device detected, auto-starting...";
        start();
    }
}

void OpenAutoController::onUsbDeviceChanged(const QString& path) {
    Q_UNUSED(path)
    // Debounce rapid USB events
    // HIGH FIX: Use QPointer to prevent use-after-free
    QPointer<OpenAutoController> weakThis(this);
    QTimer::singleShot(USB_DEBOUNCE_DELAY_MS, this, [weakThis]() {
        if (weakThis)
            weakThis->checkUsbDevices();
    });
}

// Private methods

void OpenAutoController::setError(const QString& message) {
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_errorMessage != message) {
            m_errorMessage = message;
            changed        = true;
        }
    }
    if (changed) {
        emit errorChanged();
        qWarning() << "[OpenAutoController] Error:" << message;
    }
}

void OpenAutoController::clearError() {
    bool changed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_errorMessage.isEmpty()) {
            m_errorMessage.clear();
            changed = true;
        }
    }
    if (changed) {
        emit errorChanged();
    }
}

void OpenAutoController::setConnected(bool connected) {
    QString currentPhoneName;
    bool changed = false;

    {
        QMutexLocker locker(&m_stateMutex);
        if (m_connected != connected) {
            m_connected = connected;
            changed     = true;

            if (!connected) {
                m_phoneName.clear();
                m_connectionType.clear();
            }
            currentPhoneName = m_phoneName;
        }
    }

    if (changed) {
        emit connectedChanged();

        if (connected) {
            qInfo() << "[OpenAutoController] Phone connected:" << currentPhoneName;
            emit phoneConnected(currentPhoneName);
        } else {
            emit phoneNameChanged();
            emit connectionTypeChanged();
            emit phoneDisconnected();
            qInfo() << "[OpenAutoController] Phone disconnected";
        }
    }
}

void OpenAutoController::startUsbMonitoring() {
#ifdef Q_OS_LINUX
    // Watch /dev for USB device changes
    if (QDir("/dev").exists()) {
        m_usbWatcher->addPath("/dev");
    }

    // Also watch /sys/bus/usb/devices if available
    if (QDir("/sys/bus/usb/devices").exists()) {
        m_usbWatcher->addPath("/sys/bus/usb/devices");
    }

    // Periodic check as backup
    m_usbCheckTimer->start(USB_CHECK_INTERVAL_MS);

    // Initial scan
    QTimer::singleShot(INITIAL_USB_CHECK_DELAY_MS, this, &OpenAutoController::checkUsbDevices);

    qInfo() << "USB device monitoring started";
#else
    qInfo() << "USB monitoring not implemented for this platform";
#endif
}

void OpenAutoController::stopUsbMonitoring() {
    m_usbCheckTimer->stop();
    m_usbWatcher->removePaths(m_usbWatcher->directories());
    qInfo() << "USB device monitoring stopped";
}

// MISRA 15.6 FIX: Helper function to handle device detection update
void OpenAutoController::handleDeviceDetected(const QString& devicePath,
                                              const QString& deviceName) {
    // Thread-safe state update
    bool phoneNameChanged  = false;
    bool lastDeviceChanged = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (deviceName != m_phoneName) {
            m_phoneName      = deviceName;
            phoneNameChanged = true;
        }
        if (m_lastDetectedDevice != devicePath) {
            m_lastDetectedDevice = devicePath;
            lastDeviceChanged    = true;
        }
    }

    if (phoneNameChanged) {
        emit this->phoneNameChanged();
    }
    if (lastDeviceChanged) {
        qInfo() << "[OpenAutoController] Android Auto compatible device detected:" << deviceName;
        emit this->phoneConnected(deviceName);
    }
}

bool OpenAutoController::detectAndroidAutoDevice() {
#ifdef Q_OS_LINUX
    QDir usbDir("/sys/bus/usb/devices");
    if (!usbDir.exists()) {
        return false;
    }

    QStringList entries = usbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries) {
        QString devicePath = usbDir.filePath(entry);
        QFile vendorFile(devicePath + "/idVendor");

        // MISRA 15.6 FIX: Early continue reduces nesting
        if (!vendorFile.open(QIODevice::ReadOnly))
            continue;

        QString vendor = QString::fromUtf8(vendorFile.readAll()).trimmed();
        vendorFile.close();

        // Check if it's an Android Auto device using helper function
        if (!isAndroidAutoVendor(vendor))
            continue;

        // Found an Android Auto device
        QString deviceName = getDeviceName(devicePath);
        handleDeviceDetected(devicePath, deviceName);
        return true;
    }

    // No device found - thread-safe clear
    bool wasDevicePresent = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_lastDetectedDevice.isEmpty()) {
            m_lastDetectedDevice.clear();
            wasDevicePresent = true;
        }
    }

    if (wasDevicePresent) {
        qInfo() << "[OpenAutoController] Android Auto device disconnected";
    }
#endif

    return false;
}

QString OpenAutoController::getDeviceName(const QString& devicePath) {
    QString name = "Android Device";

#ifdef Q_OS_LINUX
    QFile mfgFile(devicePath + "/manufacturer");
    QFile prodFile(devicePath + "/product");

    QString manufacturer, product;

    if (mfgFile.open(QIODevice::ReadOnly)) {
        manufacturer = QString::fromUtf8(mfgFile.readAll()).trimmed();
        mfgFile.close();
    }

    if (prodFile.open(QIODevice::ReadOnly)) {
        product = QString::fromUtf8(prodFile.readAll()).trimmed();
        prodFile.close();
    }

    if (!manufacturer.isEmpty() && !product.isEmpty()) {
        name = manufacturer + " " + product;
    } else if (!manufacturer.isEmpty()) {
        name = manufacturer;
    } else if (!product.isEmpty()) {
        name = product;
    }
#else
    Q_UNUSED(devicePath)
#endif

    return name;
}

}  // namespace speeduino

#include "hmi/openauto_controller.hpp"
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>

namespace speeduino {

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
    : QObject(parent)
    , m_process(std::make_unique<QProcess>(this))
    , m_usbCheckTimer(std::make_unique<QTimer>(this))
    , m_usbWatcher(std::make_unique<QFileSystemWatcher>(this))
{
    // Process connections
    connect(m_process.get(), &QProcess::started,
            this, &OpenAutoController::onProcessStarted);
    connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OpenAutoController::onProcessFinished);
    connect(m_process.get(), &QProcess::errorOccurred,
            this, &OpenAutoController::onProcessError);
    connect(m_process.get(), &QProcess::readyReadStandardOutput,
            this, &OpenAutoController::onReadyReadStdout);
    connect(m_process.get(), &QProcess::readyReadStandardError,
            this, &OpenAutoController::onReadyReadStderr);

    // USB monitoring
    connect(m_usbCheckTimer.get(), &QTimer::timeout,
            this, &OpenAutoController::checkUsbDevices);
    connect(m_usbWatcher.get(), &QFileSystemWatcher::directoryChanged,
            this, &OpenAutoController::onUsbDeviceChanged);

    // Start USB monitoring
    startUsbMonitoring();
}

OpenAutoController::~OpenAutoController() {
    stop();
    stopUsbMonitoring();
}

void OpenAutoController::setExecutablePath(const QString& path) {
    m_executablePath = path;
    qInfo() << "OpenAuto executable path set to:" << path;
}

void OpenAutoController::setFullscreen(bool fullscreen) {
    m_fullscreen = fullscreen;
}

void OpenAutoController::setAutoStart(bool autoStart) {
    if (m_autoStart != autoStart) {
        m_autoStart = autoStart;
        emit autoStartChanged();
        qInfo() << "OpenAuto auto-start:" << (autoStart ? "enabled" : "disabled");
    }
}

void OpenAutoController::setWirelessEnabled(bool enabled) {
    if (m_wirelessEnabled != enabled) {
        m_wirelessEnabled = enabled;
        emit wirelessEnabledChanged();
        qInfo() << "Wireless Android Auto:" << (enabled ? "enabled" : "disabled");
    }
}

void OpenAutoController::setResolution(int width, int height, int fps) {
    m_videoWidth = width;
    m_videoHeight = height;
    m_videoFps = fps;
    qInfo() << "OpenAuto resolution set to:" << width << "x" << height << "@" << fps << "fps";
}

bool OpenAutoController::start() {
    if (m_running) {
        qWarning() << "OpenAuto already running";
        return true;
    }

    clearError();

    // Check if executable exists
    QFile exe(m_executablePath);
    if (!exe.exists()) {
        // Try alternative paths
        QStringList altPaths = {
            "/usr/bin/autoapp",
            "/usr/local/bin/autoapp",
            "/opt/openauto/bin/autoapp",
            "/home/pi/openauto/bin/autoapp",
            QDir::homePath() + "/openauto/bin/autoapp"
        };

        bool found = false;
        for (const QString& path : altPaths) {
            if (QFile::exists(path)) {
                m_executablePath = path;
                found = true;
                qInfo() << "Found OpenAuto at:" << path;
                break;
            }
        }

        if (!found) {
            setError(QString("OpenAuto executable not found. Searched:\n%1\n%2")
                    .arg(m_executablePath)
                    .arg(altPaths.join("\n")));
            return false;
        }
    }

    // Build arguments
    QStringList args;

    // Resolution settings
    args << "-r" << QString("%1x%2").arg(m_videoWidth).arg(m_videoHeight);
    args << "-f" << QString::number(m_videoFps);

    if (m_fullscreen) {
        args << "--fullscreen";
    }

    // Wireless mode
    if (m_wirelessEnabled) {
        args << "--wireless";
    }

    qInfo() << "Starting OpenAuto:" << m_executablePath << args;

    m_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    m_process->start(m_executablePath, args);

    if (!m_process->waitForStarted(5000)) {
        setError("Failed to start OpenAuto process within timeout");
        return false;
    }

    return true;
}

void OpenAutoController::stop() {
    if (!m_running) {
        return;
    }

    qInfo() << "Stopping OpenAuto...";

    // Send SIGTERM first for graceful shutdown
    m_process->terminate();

    if (!m_process->waitForFinished(3000)) {
        // Force kill if doesn't respond
        qWarning() << "OpenAuto not responding to SIGTERM, sending SIGKILL...";
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    setConnected(false);
}

void OpenAutoController::restart() {
    qInfo() << "Restarting OpenAuto...";
    stop();

    // Wait a bit before restarting
    QTimer::singleShot(500, this, [this]() {
        start();
    });
}

void OpenAutoController::toggle() {
    if (m_running) {
        stop();
    } else {
        start();
    }
}

void OpenAutoController::sendTouch(int x, int y, int type) {
    if (!m_running || !m_connected) {
        return;
    }

    // Touch types: 0 = press, 1 = release, 2 = move
    // This would send touch events to the OpenAuto process
    // Implementation depends on how OpenAuto accepts input

    // For now, log the touch events (actual implementation requires
    // either IPC with OpenAuto or using an input device)
    qDebug() << "Touch event:" << x << y << "type:" << type;

    // TODO: Implement actual touch forwarding via:
    // Option 1: Write to a named pipe that OpenAuto reads
    // Option 2: Use DBus to communicate with OpenAuto
    // Option 3: Use uinput to create virtual touch device
}

void OpenAutoController::scanForDevices() {
    qInfo() << "Scanning for Android Auto devices...";
    checkUsbDevices();
}

QStringList OpenAutoController::getConnectedDevices() const {
    QStringList devices;

#ifdef Q_OS_LINUX
    // Read from /sys/bus/usb/devices
    QDir usbDir("/sys/bus/usb/devices");
    QStringList entries = usbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries) {
        QString vendorPath = usbDir.filePath(entry + "/idVendor");
        QString productPath = usbDir.filePath(entry + "/idProduct");
        QString manufacturerPath = usbDir.filePath(entry + "/manufacturer");
        QString productNamePath = usbDir.filePath(entry + "/product");

        QFile vendorFile(vendorPath);
        QFile productFile(productPath);

        if (vendorFile.open(QIODevice::ReadOnly) && productFile.open(QIODevice::ReadOnly)) {
            QString vendor = QString::fromUtf8(vendorFile.readAll()).trimmed();
            QString product = QString::fromUtf8(productFile.readAll()).trimmed();
            QString usbId = vendor + ":" + product;

            // Check if it's an Android device
            for (const QString& aaId : AA_USB_IDS) {
                if (usbId.startsWith(aaId.left(4), Qt::CaseInsensitive)) {
                    // Get device name
                    QString deviceName = "Unknown Android Device";
                    QFile mfgFile(manufacturerPath);
                    QFile prodFile(productNamePath);

                    if (mfgFile.open(QIODevice::ReadOnly)) {
                        deviceName = QString::fromUtf8(mfgFile.readAll()).trimmed();
                    }
                    if (prodFile.open(QIODevice::ReadOnly)) {
                        QString prodName = QString::fromUtf8(prodFile.readAll()).trimmed();
                        if (!prodName.isEmpty()) {
                            deviceName += " " + prodName;
                        }
                    }

                    devices << deviceName;
                    break;
                }
            }
        }
    }
#endif

    return devices;
}

// Private slots

void OpenAutoController::onProcessStarted() {
    m_running = true;
    emit runningChanged();
    emit started();
    qInfo() << "OpenAuto process started (PID:" << m_process->processId() << ")";
    emit showNotification("OpenAuto", "Android Auto is ready. Connect your phone.");
}

void OpenAutoController::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    m_running = false;
    emit runningChanged();

    setConnected(false);

    if (status == QProcess::CrashExit) {
        QString msg = QString("OpenAuto crashed with exit code %1").arg(exitCode);
        setError(msg);
        emit crashed(msg);
        emit showNotification("OpenAuto Error", msg);

        // Auto-restart on crash if autoStart is enabled
        if (m_autoStart) {
            qInfo() << "Auto-restarting OpenAuto after crash...";
            QTimer::singleShot(2000, this, &OpenAutoController::start);
        }
    } else {
        qInfo() << "OpenAuto stopped normally with exit code" << exitCode;
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
    QString output = QString::fromUtf8(data).trimmed();

    if (output.isEmpty()) return;

    qDebug() << "[OpenAuto]" << output;

    // Parse OpenAuto output for connection status
    // These strings depend on OpenAuto's actual output format
    if (output.contains("Phone connected", Qt::CaseInsensitive) ||
        output.contains("AndroidAuto started", Qt::CaseInsensitive) ||
        output.contains("Projection started", Qt::CaseInsensitive)) {
        setConnected(true);
        m_connectionType = "USB";
        emit connectionTypeChanged();
        emit showNotification("Android Auto", "Phone connected successfully!");
    }
    else if (output.contains("Phone disconnected", Qt::CaseInsensitive) ||
             output.contains("AndroidAuto stopped", Qt::CaseInsensitive) ||
             output.contains("Projection stopped", Qt::CaseInsensitive)) {
        setConnected(false);
    }
    else if (output.contains("Wireless connection", Qt::CaseInsensitive)) {
        m_connectionType = "Wireless";
        emit connectionTypeChanged();
    }
}

void OpenAutoController::onReadyReadStderr() {
    QByteArray data = m_process->readAllStandardError();
    QString output = QString::fromUtf8(data).trimmed();

    if (!output.isEmpty()) {
        qWarning() << "[OpenAuto ERROR]" << output;
    }
}

void OpenAutoController::checkUsbDevices() {
    bool deviceFound = detectAndroidAutoDevice();

    if (deviceFound && !m_connected && m_autoStart && !m_running) {
        qInfo() << "Android Auto device detected, auto-starting OpenAuto...";
        start();
    }
}

void OpenAutoController::onUsbDeviceChanged(const QString& path) {
    Q_UNUSED(path)
    // Debounce rapid USB events
    QTimer::singleShot(500, this, &OpenAutoController::checkUsbDevices);
}

// Private methods

void OpenAutoController::setError(const QString& message) {
    if (m_errorMessage != message) {
        m_errorMessage = message;
        emit errorChanged();
        qWarning() << "OpenAuto error:" << message;
    }
}

void OpenAutoController::clearError() {
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorChanged();
    }
}

void OpenAutoController::setConnected(bool connected) {
    if (m_connected != connected) {
        m_connected = connected;
        emit connectedChanged();

        if (connected) {
            qInfo() << "Android Auto phone connected:" << m_phoneName;
            emit phoneConnected(m_phoneName);
        } else {
            m_phoneName.clear();
            m_connectionType.clear();
            emit phoneNameChanged();
            emit connectionTypeChanged();
            emit phoneDisconnected();
            qInfo() << "Android Auto phone disconnected";
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

    // Periodic check as backup (every 2 seconds)
    m_usbCheckTimer->start(2000);

    // Initial scan
    QTimer::singleShot(100, this, &OpenAutoController::checkUsbDevices);

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

bool OpenAutoController::detectAndroidAutoDevice() {
#ifdef Q_OS_LINUX
    QDir usbDir("/sys/bus/usb/devices");
    if (!usbDir.exists()) {
        return false;
    }

    QStringList entries = usbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries) {
        QString vendorPath = usbDir.filePath(entry + "/idVendor");
        QFile vendorFile(vendorPath);

        if (vendorFile.open(QIODevice::ReadOnly)) {
            QString vendor = QString::fromUtf8(vendorFile.readAll()).trimmed();
            vendorFile.close();

            // Check against known Android AA vendor IDs
            for (const QString& aaId : AA_USB_IDS) {
                if (vendor.compare(aaId.left(4), Qt::CaseInsensitive) == 0) {
                    // Found a potential Android Auto device
                    QString devicePath = usbDir.filePath(entry);
                    QString deviceName = getDeviceName(devicePath);

                    if (deviceName != m_phoneName) {
                        m_phoneName = deviceName;
                        emit phoneNameChanged();
                    }

                    if (m_lastDetectedDevice != devicePath) {
                        m_lastDetectedDevice = devicePath;
                        qInfo() << "Android Auto compatible device detected:" << deviceName;
                    }

                    return true;
                }
            }
        }
    }

    // No device found
    if (!m_lastDetectedDevice.isEmpty()) {
        m_lastDetectedDevice.clear();
        qInfo() << "Android Auto device disconnected";
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

} // namespace speeduino

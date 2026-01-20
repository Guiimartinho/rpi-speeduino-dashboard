#ifndef HMI_OPENAUTO_CONTROLLER_HPP
#define HMI_OPENAUTO_CONTROLLER_HPP

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include <chrono>

namespace speeduino {

/**
 * @brief OpenAutoController manages the OpenAuto/Android Auto integration
 *
 * This controller handles:
 * - Starting/stopping OpenAuto process
 * - USB device detection for Android phones
 * - Phone connection state management
 * - Touch event forwarding
 * - Wireless Android Auto preparation
 */
class OpenAutoController : public QObject {
    Q_OBJECT

    // Process state
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

    // Connection info
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY connectionTypeChanged)
    Q_PROPERTY(QString phoneName READ phoneName NOTIFY phoneNameChanged)

    // Configuration
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(bool wirelessEnabled READ wirelessEnabled WRITE setWirelessEnabled NOTIFY wirelessEnabledChanged)

public:
    explicit OpenAutoController(QObject* parent = nullptr);
    ~OpenAutoController();

    // Thread-safe property getters
    bool isRunning() const;
    bool isConnected() const;
    QString errorMessage() const;
    QString connectionType() const;
    QString phoneName() const;
    bool autoStart() const;
    bool wirelessEnabled() const;

    // Configuration
    Q_INVOKABLE void setExecutablePath(const QString& path);
    Q_INVOKABLE void setFullscreen(bool fullscreen);
    Q_INVOKABLE void setAutoStart(bool autoStart);
    Q_INVOKABLE void setWirelessEnabled(bool enabled);
    Q_INVOKABLE void setResolution(int width, int height, int fps = 60);

    // Control
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void toggle();

    // Touch event forwarding (for embedded mode)
    Q_INVOKABLE void sendTouch(int x, int y, int type);

    // USB device scanning
    Q_INVOKABLE void scanForDevices();
    Q_INVOKABLE QStringList getConnectedDevices() const;

signals:
    // State signals
    void runningChanged();
    void connectedChanged();
    void errorChanged();
    void connectionTypeChanged();
    void phoneNameChanged();
    void autoStartChanged();
    void wirelessEnabledChanged();

    // Event signals
    void started();
    void stopped();
    void crashed(const QString& message);
    void phoneConnected(const QString& deviceName);
    void phoneDisconnected();

    // For UI notifications
    void showNotification(const QString& title, const QString& message);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStdout();
    void onReadyReadStderr();

    // USB monitoring
    void checkUsbDevices();
    void onUsbDeviceChanged(const QString& path);

private:
    void setError(const QString& message);
    void clearError();
    void setConnected(bool connected);
    void startUsbMonitoring();
    void stopUsbMonitoring();
    bool detectAndroidAutoDevice();
    QString getDeviceName(const QString& devicePath);

    // FIX: Orphan process cleanup to prevent "Address already in use" errors
    void killOrphanProcesses();
    void resetBluetoothProfile();

    // MISRA 15.6 FIX: Helper functions to reduce nesting depth
    bool isAndroidAutoVendor(const QString& vendorId) const;
    QString readDeviceInfo(const QString& devicePath) const;
    void handleDeviceDetected(const QString& devicePath, const QString& deviceName);

    // Thread synchronization - protects state variables
    mutable QMutex m_stateMutex;

    // Configuration (protected by m_stateMutex)
    QString m_executablePath{"/usr/local/bin/autoapp"};
    bool m_fullscreen{true};
    bool m_autoStart{true};
    bool m_wirelessEnabled{false};
    int m_videoWidth{800};
    int m_videoHeight{480};
    int m_videoFps{60};

    // State (protected by m_stateMutex)
    bool m_running{false};
    bool m_connected{false};
    QString m_errorMessage;
    QString m_connectionType;  // "USB" or "Wireless"
    QString m_phoneName;

    // Process management
    std::unique_ptr<QProcess> m_process;

    // USB monitoring
    std::unique_ptr<QTimer> m_usbCheckTimer;
    std::unique_ptr<QFileSystemWatcher> m_usbWatcher;
    QString m_lastDetectedDevice;

    // FIX #9: Crash loop prevention (ISO 26262)
    // Limits automatic restarts to prevent infinite crash loops
    static constexpr int MAX_CRASH_RESTARTS = 3;
    static constexpr int INITIAL_RESTART_DELAY_MS = 2000;
    static constexpr int MAX_RESTART_DELAY_MS = 30000;
    int m_crashRestartCount{0};
    int m_currentRestartDelayMs{INITIAL_RESTART_DELAY_MS};
    std::chrono::steady_clock::time_point m_lastSuccessfulStart;

    // Android Auto USB identifiers (vendor:product)
    static const QStringList AA_USB_IDS;
};

} // namespace speeduino

#endif // HMI_OPENAUTO_CONTROLLER_HPP

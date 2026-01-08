#ifndef HMI_OPENAUTO_CONTROLLER_HPP
#define HMI_OPENAUTO_CONTROLLER_HPP

#include <QObject>
#include <QString>
#include <QProcess>
#include <memory>

namespace speeduino {

class OpenAutoController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

public:
    explicit OpenAutoController(QObject* parent = nullptr);
    ~OpenAutoController();

    bool isRunning() const { return m_running; }
    QString errorMessage() const { return m_errorMessage; }

    // Configuration
    Q_INVOKABLE void setExecutablePath(const QString& path);
    Q_INVOKABLE void setFullscreen(bool fullscreen);

    // Control
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();

signals:
    void runningChanged();
    void errorChanged();
    void started();
    void stopped();
    void crashed(const QString& message);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStdout();
    void onReadyReadStderr();

private:
    void setError(const QString& message);
    void clearError();

    QString m_executablePath{"/usr/local/bin/openauto"};
    bool m_fullscreen{true};
    bool m_running{false};
    QString m_errorMessage;

    std::unique_ptr<QProcess> m_process;
};

} // namespace speeduino

#endif // HMI_OPENAUTO_CONTROLLER_HPP

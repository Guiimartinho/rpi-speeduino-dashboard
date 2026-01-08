#include "hmi/openauto_controller.hpp"
#include <QDebug>
#include <QFile>

namespace speeduino {

OpenAutoController::OpenAutoController(QObject* parent)
    : QObject(parent)
    , m_process(std::make_unique<QProcess>(this))
{
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
}

OpenAutoController::~OpenAutoController() {
    stop();
}

void OpenAutoController::setExecutablePath(const QString& path) {
    m_executablePath = path;
}

void OpenAutoController::setFullscreen(bool fullscreen) {
    m_fullscreen = fullscreen;
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
        setError(QString("OpenAuto executable not found: %1").arg(m_executablePath));
        return false;
    }

    // Build arguments
    QStringList args;
    if (m_fullscreen) {
        args << "--fullscreen";
    }

    qInfo() << "Starting OpenAuto:" << m_executablePath << args;

    m_process->start(m_executablePath, args);

    if (!m_process->waitForStarted(5000)) {
        setError("Failed to start OpenAuto process");
        return false;
    }

    return true;
}

void OpenAutoController::stop() {
    if (!m_running) {
        return;
    }

    qInfo() << "Stopping OpenAuto...";

    // Send SIGTERM first
    m_process->terminate();

    if (!m_process->waitForFinished(3000)) {
        // Force kill if doesn't respond
        qWarning() << "OpenAuto not responding, killing...";
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void OpenAutoController::toggle() {
    if (m_running) {
        stop();
    } else {
        start();
    }
}

void OpenAutoController::onProcessStarted() {
    m_running = true;
    emit runningChanged();
    emit started();
    qInfo() << "OpenAuto started";
}

void OpenAutoController::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    m_running = false;
    emit runningChanged();
    emit stopped();

    if (status == QProcess::CrashExit) {
        QString msg = QString("OpenAuto crashed with exit code %1").arg(exitCode);
        setError(msg);
        emit crashed(msg);
    } else {
        qInfo() << "OpenAuto stopped with exit code" << exitCode;
    }
}

void OpenAutoController::onProcessError(QProcess::ProcessError error) {
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start OpenAuto - check executable path";
            break;
        case QProcess::Crashed:
            errorMsg = "OpenAuto process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "OpenAuto process timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Write error communicating with OpenAuto";
            break;
        case QProcess::ReadError:
            errorMsg = "Read error communicating with OpenAuto";
            break;
        default:
            errorMsg = "Unknown OpenAuto error";
            break;
    }

    setError(errorMsg);
}

void OpenAutoController::onReadyReadStdout() {
    QByteArray data = m_process->readAllStandardOutput();
    qDebug() << "[OpenAuto]" << data.trimmed();
}

void OpenAutoController::onReadyReadStderr() {
    QByteArray data = m_process->readAllStandardError();
    qWarning() << "[OpenAuto ERROR]" << data.trimmed();
}

void OpenAutoController::setError(const QString& message) {
    m_errorMessage = message;
    emit errorChanged();
    qWarning() << "OpenAuto error:" << message;
}

void OpenAutoController::clearError() {
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorChanged();
    }
}

} // namespace speeduino

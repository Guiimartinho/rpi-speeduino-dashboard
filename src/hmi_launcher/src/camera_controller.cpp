#include "hmi/camera_controller.hpp"
#include <QDebug>

namespace speeduino {

CameraController::CameraController(QObject* parent)
    : QObject(parent)
{
}

CameraController::~CameraController() {
    stop();
}

void CameraController::setDevice(const QString& device) {
    m_device = device;
}

void CameraController::setResolution(int width, int height) {
    m_width = width;
    m_height = height;
}

void CameraController::setFramerate(int fps) {
    m_fps = fps;
}

QString CameraController::pipelineDescription() const {
    return buildGstreamerPipeline();
}

QString CameraController::buildGstreamerPipeline() const {
    // Low-latency GStreamer pipeline for reverse camera
    // Uses v4l2src with minimal buffering
    QString pipeline = QString(
        "v4l2src device=%1 ! "
        "video/x-raw,width=%2,height=%3,framerate=%4/1 ! "
        "videoconvert ! "
        "autovideosink sync=false"
    ).arg(m_device)
     .arg(m_width)
     .arg(m_height)
     .arg(m_fps);

    return pipeline;
}

bool CameraController::start() {
    if (m_active) {
        qWarning() << "Camera already active";
        return true;
    }

    clearError();

    // For Qt6, we use QtMultimedia's MediaPlayer/VideoOutput in QML
    // This controller just manages state and provides configuration
    // The actual video display happens in QML

    // Check if device exists
    QFile device(m_device);
    if (!device.exists()) {
        setError(QString("Camera device not found: %1").arg(m_device));
        return false;
    }

    m_active = true;
    emit activeChanged();
    emit cameraReady();

    qInfo() << "Camera controller started:" << m_device;
    return true;
}

void CameraController::stop() {
    if (!m_active) {
        return;
    }

    m_active = false;
    emit activeChanged();

    qInfo() << "Camera controller stopped";
}

void CameraController::onProcessStarted() {
    m_active = true;
    emit activeChanged();
    emit cameraReady();
}

void CameraController::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    m_active = false;
    emit activeChanged();

    if (status == QProcess::CrashExit) {
        setError(QString("Camera process crashed with code %1").arg(exitCode));
    }
}

void CameraController::onProcessError(QProcess::ProcessError error) {
    m_active = false;
    emit activeChanged();

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start camera process";
            break;
        case QProcess::Crashed:
            errorMsg = "Camera process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Camera process timed out";
            break;
        default:
            errorMsg = "Unknown camera error";
            break;
    }

    setError(errorMsg);
    emit cameraError(errorMsg);
}

void CameraController::setError(const QString& message) {
    m_errorMessage = message;
    emit errorChanged();
    qWarning() << "Camera error:" << message;
}

void CameraController::clearError() {
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorChanged();
    }
}

} // namespace speeduino

#ifndef HMI_CAMERA_CONTROLLER_HPP
#define HMI_CAMERA_CONTROLLER_HPP

#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

namespace speeduino {

class CameraController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString pipelineDescription READ pipelineDescription CONSTANT)

public:
    explicit CameraController(QObject* parent = nullptr);
    ~CameraController();

    bool isActive() const { return m_active; }
    QString errorMessage() const { return m_errorMessage; }
    QString pipelineDescription() const;

    // Camera device configuration
    Q_INVOKABLE void setDevice(const QString& device);
    Q_INVOKABLE void setResolution(int width, int height);
    Q_INVOKABLE void setFramerate(int fps);

    // Start/stop camera
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

signals:
    void activeChanged();
    void errorChanged();
    void cameraReady();
    void cameraError(const QString& message);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    QString buildGstreamerPipeline() const;
    void setError(const QString& message);
    void clearError();

    QString m_device{"/dev/video0"};
    int m_width{640};
    int m_height{480};
    int m_fps{30};

    bool m_active{false};
    QString m_errorMessage;

    std::unique_ptr<QProcess> m_process;
};

}  // namespace speeduino

#endif  // HMI_CAMERA_CONTROLLER_HPP

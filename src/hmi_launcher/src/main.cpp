#include "hmi/data_provider.hpp"
#include "hmi/camera_controller.hpp"
#include "hmi/openauto_controller.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char *argv[])
{
    // Enable high DPI scaling
    QGuiApplication app(argc, argv);

    app.setApplicationName("Speeduino UI");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Speeduino");

    // Command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription("Speeduino Dashboard and Multimedia Launcher");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(QStringList() << "c" << "config",
        "Config directory", "dir", "/etc/speeduino-ui");
    parser.addOption(configOption);

    QCommandLineOption fullscreenOption(QStringList() << "f" << "fullscreen",
        "Run in fullscreen mode");
    parser.addOption(fullscreenOption);

    QCommandLineOption debugOption(QStringList() << "d" << "debug",
        "Enable debug output");
    parser.addOption(debugOption);

    parser.process(app);

    bool fullscreen = parser.isSet(fullscreenOption);
    bool debug = parser.isSet(debugOption);
    QString configDir = parser.value(configOption);

    if (debug) {
        qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] %{message}");
    }

    qInfo() << "Speeduino UI starting...";
    qInfo() << "Config dir:" << configDir;
    qInfo() << "Fullscreen:" << fullscreen;

    // Set Qt Quick style
    QQuickStyle::setStyle("Basic");

    // Create QML engine
    QQmlApplicationEngine engine;

    // Create controllers
    speeduino::DataProvider dataProvider;
    speeduino::CameraController cameraController;
    speeduino::OpenAutoController openAutoController;

    // Configure from settings (could load from YAML)
    cameraController.setDevice("/dev/video0");
    cameraController.setResolution(640, 480);
    cameraController.setFramerate(30);

    openAutoController.setExecutablePath("/usr/local/bin/openauto");
    openAutoController.setFullscreen(true);

    // Expose to QML
    engine.rootContext()->setContextProperty("dataProvider", &dataProvider);
    engine.rootContext()->setContextProperty("cameraController", &cameraController);
    engine.rootContext()->setContextProperty("openAutoController", &openAutoController);
    engine.rootContext()->setContextProperty("isFullscreen", fullscreen);

    // Load main QML
    const QUrl url(u"qrc:/SpeeduinoUI/qml/main.qml"_qs);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML";
        return -1;
    }

    // Start data provider
    dataProvider.start();

    qInfo() << "Speeduino UI started";

    int result = app.exec();

    // Cleanup
    dataProvider.stop();
    cameraController.stop();
    openAutoController.stop();

    qInfo() << "Speeduino UI stopped";
    return result;
}

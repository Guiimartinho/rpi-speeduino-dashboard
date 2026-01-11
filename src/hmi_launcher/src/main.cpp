#include "hmi/data_provider.hpp"
#include "hmi/camera_controller.hpp"
#include "hmi/openauto_controller.hpp"
#include "hmi/openauto_embedded.hpp"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QCommandLineParser>
#include <QDebug>

/**
 * main.cpp - Speeduino UI Launcher
 *
 * ARCHITECTURE:
 * This application provides an embedded automotive dashboard with Android Auto integration.
 * The Android Auto video is rendered INSIDE the UI (not fullscreen) with tab navigation
 * remaining accessible at the bottom.
 *
 * VIDEO EMBEDDING APPROACH:
 * - OpenAutoEmbedded creates a QWidget for video output
 * - When user navigates to OpenAutoScreen, QML calls setVideoContainer()
 * - C++ parents the video widget to the QML window and positions it over the container
 * - QML notifies C++ of visibility changes via setVideoVisible()
 * - This reactive approach eliminates startup timing issues
 */
int main(int argc, char *argv[])
{
    // Use QApplication for QWidget support (required by embedded OpenAuto)
    QApplication app(argc, argv);

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

    // Option to use process-based OpenAuto instead of embedded
    QCommandLineOption processOpenAutoOption(QStringList() << "process-openauto",
        "Use process-based OpenAuto instead of embedded");
    parser.addOption(processOpenAutoOption);

    parser.process(app);

    bool fullscreen = parser.isSet(fullscreenOption);
    bool debug = parser.isSet(debugOption);
    bool useProcessOpenAuto = parser.isSet(processOpenAutoOption);
    QString configDir = parser.value(configOption);

    if (debug) {
        qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] %{message}");
    }

    qInfo() << "Speeduino UI starting...";
    qInfo() << "Config dir:" << configDir;
    qInfo() << "Fullscreen:" << fullscreen;
    qInfo() << "Use embedded OpenAuto:" << !useProcessOpenAuto;

    // Set Qt Quick style
    QQuickStyle::setStyle("Basic");

    // Create QML engine
    QQmlApplicationEngine engine;

    // Create controllers
    speeduino::DataProvider dataProvider;
    speeduino::CameraController cameraController;
    speeduino::OpenAutoController openAutoController;
    speeduino::OpenAutoEmbedded openAutoEmbedded;

    // Configure camera
    cameraController.setDevice("/dev/video0");
    cameraController.setResolution(640, 480);
    cameraController.setFramerate(30);

    // Configure process-based OpenAuto (fallback mode)
    // Only used if --process-openauto flag is passed
    openAutoController.setExecutablePath("/usr/local/bin/openauto");
    openAutoController.setFullscreen(false);  // CRITICAL: Never fullscreen!

    // Configure embedded OpenAuto
    // Content area is 800x480 minus tab bar height (60px) = 800x420
    openAutoEmbedded.setResolution(800, 420);

    // Expose controllers to QML
    engine.rootContext()->setContextProperty("dataProvider", &dataProvider);
    engine.rootContext()->setContextProperty("cameraController", &cameraController);
    engine.rootContext()->setContextProperty("openAutoController", &openAutoController);
    engine.rootContext()->setContextProperty("openAutoEmbedded", &openAutoEmbedded);
    engine.rootContext()->setContextProperty("isFullscreen", fullscreen);
    engine.rootContext()->setContextProperty("useProcessOpenAuto", useProcessOpenAuto);

    // Load main QML
    // Using Qt::StringLiterals for modern Qt6 compatibility
    using namespace Qt::StringLiterals;
    const QUrl url(u"qrc:/SpeeduinoUI/qml/main.qml"_s);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML";
        return -1;
    }

    // Get the root window for reference
    QQuickWindow* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (rootWindow) {
        qInfo() << "Root window created:" << rootWindow->width() << "x" << rootWindow->height();

        // NOTE: Video widget integration is now handled reactively by QML:
        // 1. OpenAutoScreen.qml calls openAutoEmbedded.setVideoContainer() when it loads
        // 2. OpenAutoScreen.qml calls openAutoEmbedded.setVideoVisible() on visibility changes
        // 3. This eliminates timing issues with the old startup-based lookup approach
    } else {
        qCritical() << "Failed to get root window";
    }

    // Start data provider
    dataProvider.start();

    qInfo() << "Speeduino UI started";

    int result = app.exec();

    // Cleanup
    dataProvider.stop();
    cameraController.stop();
    openAutoController.stop();
    openAutoEmbedded.stop();

    qInfo() << "Speeduino UI stopped";
    return result;
}

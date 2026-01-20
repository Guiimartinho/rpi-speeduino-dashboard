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
 *
 * ISO 26262 CONSIDERATIONS:
 * - Structured cleanup ensures resources are released on all exit paths
 * - Error handling provides diagnostics for debugging
 */

// RAII helper for cleanup - ensures resources are released on all exit paths
class ApplicationCleanup {
public:
    ApplicationCleanup(speeduino::DataProvider& dp,
                       speeduino::CameraController& cc,
                       speeduino::OpenAutoController& oac,
                       speeduino::OpenAutoEmbedded& oae)
        : m_dataProvider(dp)
        , m_cameraController(cc)
        , m_openAutoController(oac)
        , m_openAutoEmbedded(oae)
    {}

    ~ApplicationCleanup() {
        qInfo() << "[Main] Performing cleanup...";
        m_dataProvider.stop();
        m_cameraController.stop();
        m_openAutoController.stop();
        m_openAutoEmbedded.stop();
        qInfo() << "[Main] Cleanup complete";
    }

private:
    speeduino::DataProvider& m_dataProvider;
    speeduino::CameraController& m_cameraController;
    speeduino::OpenAutoController& m_openAutoController;
    speeduino::OpenAutoEmbedded& m_openAutoEmbedded;
};

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
    // NOTE: Embedded mode renders inside QML VideoOutput (REQUIRED for in-app display)
    // Process mode creates separate window (cannot be embedded in our UI)
    QCommandLineOption processOpenAutoOption(QStringList() << "process-openauto",
        "Use process-based OpenAuto (separate window, not embedded in UI)");
    parser.addOption(processOpenAutoOption);

    parser.process(app);

    const bool fullscreen = parser.isSet(fullscreenOption);
    const bool debug = parser.isSet(debugOption);
    // Default to embedded mode (useProcessOpenAuto = false) - ONLY way to render inside QML
    const bool useProcessOpenAuto = parser.isSet(processOpenAutoOption);
    const QString configDir = parser.value(configOption);

    if (debug) {
        qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] %{message}");
    }

    qInfo() << "[Main] Speeduino UI starting...";
    qInfo() << "[Main] Config dir:" << configDir;
    qInfo() << "[Main] Fullscreen:" << fullscreen;
    qInfo() << "[Main] Use process OpenAuto:" << useProcessOpenAuto;

    // Set Qt Quick style
    QQuickStyle::setStyle("Basic");

    // Create QML engine
    QQmlApplicationEngine engine;

    // Create controllers
    speeduino::DataProvider dataProvider;
    speeduino::CameraController cameraController;
    speeduino::OpenAutoController openAutoController;
    speeduino::OpenAutoEmbedded openAutoEmbedded;

    // RAII cleanup - ensures resources are released even on early exit
    ApplicationCleanup cleanup(dataProvider, cameraController,
                               openAutoController, openAutoEmbedded);

    // Configure camera
    cameraController.setDevice("/dev/video0");
    cameraController.setResolution(640, 480);
    cameraController.setFramerate(30);

    // Configure process-based OpenAuto
    // Default mode since embedded has stability issues with QMLVideoOutput
    openAutoController.setExecutablePath("/usr/local/bin/autoapp");
    openAutoController.setFullscreen(false);  // CRITICAL: Never fullscreen!
    // Content area is 800x480 minus StatusBar (36px) and TabBar (64px) = 800x380
    openAutoController.setResolution(800, 380, 30);

    // ALWAYS disable auto-start - user controls via Start button in OpenAutoScreen
    openAutoController.setAutoStart(false);
    qInfo() << "[Main] OpenAutoController auto-start disabled (user controls via UI)";

    // ALWAYS connect USB detection bridge - OpenAutoController monitors USB devices
    // and can help OpenAutoEmbedded detect phones (libusb hotplug is unreliable on some systems)
    QObject::connect(&openAutoController, &speeduino::OpenAutoController::phoneConnected,
                     &openAutoEmbedded, [&openAutoEmbedded](const QString& device) {
                         qInfo() << "[Main] USB detection bridge: phone connected -" << device;
                         openAutoEmbedded.retryDeviceDetection();
                     });
    qInfo() << "[Main] USB detection bridge connected (Controller -> Embedded)";

    // Configure embedded OpenAuto
    // Content area is 800x480 minus StatusBar (36px) and TabBar (64px) = 800x380
    openAutoEmbedded.setResolution(800, 380);

    // Expose controllers to QML
    QQmlContext* rootContext = engine.rootContext();
    if (!rootContext) {
        qCritical() << "[Main] Failed to get root context";
        return -1;  // cleanup happens via RAII
    }

    rootContext->setContextProperty("dataProvider", &dataProvider);
    rootContext->setContextProperty("cameraController", &cameraController);
    rootContext->setContextProperty("openAutoController", &openAutoController);
    rootContext->setContextProperty("openAutoEmbedded", &openAutoEmbedded);
    rootContext->setContextProperty("isFullscreen", fullscreen);
    rootContext->setContextProperty("useProcessOpenAuto", useProcessOpenAuto);

    // Load main QML
    // Using Qt::StringLiterals for modern Qt6 compatibility
    using namespace Qt::StringLiterals;
    const QUrl url(u"qrc:/SpeeduinoUI/qml/main.qml"_s);

    // Track loading errors
    bool loadFailed = false;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [&loadFailed]() {
            qCritical() << "[Main] QML object creation failed";
            loadFailed = true;
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "[Main] Failed to load QML - no root objects created";
        return -1;  // cleanup happens via RAII
    }

    // Get the root window for reference
    QQuickWindow* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (rootWindow) {
        qInfo() << "[Main] Root window created:" << rootWindow->width() << "x" << rootWindow->height();

        // NOTE: Video widget integration is now handled reactively by QML:
        // 1. OpenAutoScreen.qml calls openAutoEmbedded.setVideoContainer() when it loads
        // 2. OpenAutoScreen.qml calls openAutoEmbedded.setVideoVisible() on visibility changes
        // 3. This eliminates timing issues with the old startup-based lookup approach
    } else {
        qWarning() << "[Main] Failed to get root window - UI may not function correctly";
        // Continue anyway as this might not be fatal
    }

    // Start data provider
    dataProvider.start();

    qInfo() << "[Main] Speeduino UI started successfully";

    const int result = app.exec();

    qInfo() << "[Main] Speeduino UI exiting with code" << result;
    // cleanup happens automatically via RAII (ApplicationCleanup destructor)
    return result;
}

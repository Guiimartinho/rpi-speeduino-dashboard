#include "hmi/branding_manager.hpp"
#include "hmi/camera_controller.hpp"
#include "hmi/data_provider.hpp"
#include "hmi/openauto_embedded.hpp"
#include "hmi/system_monitor.hpp"

#include "common/config_loader.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>

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
    ApplicationCleanup(speeduino::DataProvider& dp, speeduino::CameraController& cc,
                       speeduino::OpenAutoEmbedded& oae)
        : m_dataProvider(dp), m_cameraController(cc), m_openAutoEmbedded(oae) {}

    ~ApplicationCleanup() {
        qInfo() << "[Main] Performing cleanup...";
        m_dataProvider.stop();
        m_cameraController.stop();
        m_openAutoEmbedded.stop();
        qInfo() << "[Main] Cleanup complete";
    }

private:
    speeduino::DataProvider& m_dataProvider;
    speeduino::CameraController& m_cameraController;
    speeduino::OpenAutoEmbedded& m_openAutoEmbedded;
};

int main(int argc, char* argv[]) {
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

    QCommandLineOption configOption(QStringList() << "c"
                                                  << "config",
                                    "Config directory", "dir", "/etc/speeduino-ui");
    parser.addOption(configOption);

    QCommandLineOption fullscreenOption(QStringList() << "f"
                                                      << "fullscreen",
                                        "Run in fullscreen mode");
    parser.addOption(fullscreenOption);

    QCommandLineOption debugOption(QStringList() << "d"
                                                 << "debug",
                                   "Enable debug output");
    parser.addOption(debugOption);

    parser.process(app);

    const bool fullscreen   = parser.isSet(fullscreenOption);
    const bool debug        = parser.isSet(debugOption);
    const QString configDir = parser.value(configOption);

    if (debug) {
        qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] %{message}");
    }

    qInfo() << "[Main] Speeduino UI starting...";
    qInfo() << "[Main] Config dir:" << configDir;
    qInfo() << "[Main] Fullscreen:" << fullscreen;
    qInfo() << "[Main] OpenAuto mode: EMBEDDED (always)";

    // Load system configuration (branding, CAN, camera, etc.)
    const std::string configPath = configDir.toStdString() + "/system.yaml";
    qInfo() << "[Main] Loading config:" << configPath.c_str();
    speeduino::ConfigLoader::loadSystemConfig(configPath);

    // Set Qt Quick style
    QQuickStyle::setStyle("Basic");

    // Create QML engine
    QQmlApplicationEngine engine;

    // Create controllers
    speeduino::DataProvider dataProvider;
    speeduino::CameraController cameraController;
    speeduino::OpenAutoEmbedded openAutoEmbedded;
    speeduino::BrandingManager brandingManager;
    hmi::SystemMonitor systemMonitor;

    // RAII cleanup - ensures resources are released even on early exit
    ApplicationCleanup cleanup(dataProvider, cameraController, openAutoEmbedded);

    // Configure camera from system.yaml
    const auto& sysConfig = speeduino::ConfigLoader::getSystemConfig();
    cameraController.setDevice(QString::fromStdString(sysConfig.camera_device));
    cameraController.setVideoStandard(QString::fromStdString(sysConfig.camera_standard));
    cameraController.setResolution(sysConfig.camera_width, sysConfig.camera_height);
    cameraController.setFramerate(sysConfig.camera_fps);
    cameraController.setCompositeInput(sysConfig.camera_input);

    // Configure test mode (simulated camera when no hardware available)
    if (sysConfig.camera_test_mode) {
        cameraController.setTestMode(true);
        cameraController.setTestPattern(QString::fromStdString(sysConfig.camera_test_pattern));
        qInfo() << "[Main] Camera TEST MODE enabled with pattern:"
                << sysConfig.camera_test_pattern.c_str();
    } else {
        qInfo() << "[Main] Camera configured:" << sysConfig.camera_device.c_str()
                << sysConfig.camera_standard.c_str()
                << sysConfig.camera_width << "x" << sysConfig.camera_height
                << "@" << sysConfig.camera_fps << "fps";
    }

    // Configure parking guide lines from system.yaml
    cameraController.setShowGuides(sysConfig.camera_show_guides);
    cameraController.setGuideBottomWidth(sysConfig.guide_bottom_width);
    cameraController.setGuideTopWidth(sysConfig.guide_top_width);
    cameraController.setGuideBottomY(sysConfig.guide_bottom_y);
    cameraController.setGuideTopY(sysConfig.guide_top_y);
    cameraController.setGuideDistance1(sysConfig.guide_distance_1);
    cameraController.setGuideDistance2(sysConfig.guide_distance_2);
    cameraController.setGuideDistance3(sysConfig.guide_distance_3);

    // Configure embedded OpenAuto (ALWAYS embedded, never process-based)
    // NOTE: Use VIDEO resolution (800x480), not container size (800x380).
    // QML transforms touch from container space to video space - C++ must use video dimensions.
    openAutoEmbedded.setResolution(800, 480);

    // Expose controllers to QML
    QQmlContext* rootContext = engine.rootContext();
    if (!rootContext) {
        qCritical() << "[Main] Failed to get root context";
        return -1;  // cleanup happens via RAII
    }

    rootContext->setContextProperty("dataProvider", &dataProvider);
    rootContext->setContextProperty("cameraController", &cameraController);
    rootContext->setContextProperty("openAutoEmbedded", &openAutoEmbedded);
    rootContext->setContextProperty("brandingManager", &brandingManager);
    rootContext->setContextProperty("systemMonitor", &systemMonitor);
    rootContext->setContextProperty("isFullscreen", fullscreen);

    // Load main QML
    // Using Qt::StringLiterals for modern Qt6 compatibility
    using namespace Qt::StringLiterals;
    const QUrl url(u"qrc:/SpeeduinoUI/qml/main.qml"_s);

    // Track loading errors
    bool loadFailed = false;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [&loadFailed]() {
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
        qInfo() << "[Main] Root window created:" << rootWindow->width() << "x"
                << rootWindow->height();

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

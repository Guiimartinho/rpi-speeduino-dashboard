set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Find required packages for OpenAuto integration
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# Find Boost (required by openauto/aasdk)
find_package(Boost REQUIRED COMPONENTS system log)

qt_add_executable(hmi_launcher
    src/main.cpp
    src/data_provider.cpp
    src/camera_controller.cpp
    src/openauto_controller.cpp
    src/openauto_embedded.cpp
    # Headers with Q_OBJECT macro (required for AUTOMOC)
    include/hmi/data_provider.hpp
    include/hmi/camera_controller.hpp
    include/hmi/openauto_controller.hpp
    include/hmi/openauto_embedded.hpp
)

qt_add_qml_module(hmi_launcher
    URI SpeeduinoUI
    VERSION 1.0
    QML_FILES
        # Main entry point
        qml/main.qml

        # Styles module (singleton)
        qml/styles/qmldir
        qml/styles/Theme.qml

        # State module (singleton)
        qml/state/qmldir
        qml/state/AppState.qml

        # Components module
        qml/components/qmldir
        qml/components/TabBar.qml
        qml/components/TabButton.qml
        qml/components/StatusBar.qml
        qml/components/SafeGauge.qml
        qml/components/StatusIndicator.qml
        qml/components/Gauge.qml
        qml/components/WarningIndicator.qml
        qml/components/IconButton.qml

        # Core gauges
        qml/components/ArcGauge.qml
        qml/components/CompactGauge.qml

        # Display components
        qml/components/ShiftLightBar.qml
        qml/components/GearIndicator.qml
        qml/components/NavBar.qml

        # Advanced gauges and displays
        qml/components/GForceMeter.qml
        qml/components/ChartGauge.qml
        qml/components/VEMapDisplay.qml

        # Timing and performance components
        qml/components/LapTimer.qml
        qml/components/PerformanceMeter.qml

        # Data management components
        qml/components/DataLogger.qml
        qml/components/PeakRecall.qml
        qml/components/TripComputer.qml

        # Singleton managers
        qml/components/NightModeManager.qml
        qml/components/WarningManager.qml
        qml/components/DisplayModeManager.qml

        # Screens module
        qml/screens/qmldir
        qml/screens/HomeScreen.qml
        qml/screens/DashScreen.qml
        qml/screens/RacingDashScreen.qml
        qml/screens/TuningDashScreen.qml
        qml/screens/ConfigScreen.qml
        qml/screens/OpenAutoScreen.qml
        qml/screens/ReverseOverlay.qml

        # Legacy screens (to be removed after migration)
        qml/Home.qml
        qml/Dash.qml
        qml/ReverseCamera.qml
        qml/Settings.qml
)

target_include_directories(hmi_launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZMQ_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    /usr/local/include
)

target_link_libraries(hmi_launcher PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Widgets
    Qt6::Bluetooth
    common
    ${ZMQ_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ${Boost_LIBRARIES}
    openauto
    aasdk
)

# Install
install(TARGETS hmi_launcher RUNTIME DESTINATION /usr/local/bin)
install(DIRECTORY qml/ DESTINATION /usr/share/speeduino-ui/qml)

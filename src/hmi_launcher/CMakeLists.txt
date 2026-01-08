set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(hmi_launcher
    src/main.cpp
    src/data_provider.cpp
    src/camera_controller.cpp
    src/openauto_controller.cpp
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

        # Screens module
        qml/screens/qmldir
        qml/screens/HomeScreen.qml
        qml/screens/DashScreen.qml
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
)

target_link_libraries(hmi_launcher PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Multimedia
    common
    ${ZMQ_LIBRARIES}
)

# Qt Quick Compiler for release builds (precompiles QML)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    qt_add_qml_module(hmi_launcher
        PRECOMPILE
    )
endif()

# Install
install(TARGETS hmi_launcher RUNTIME DESTINATION /usr/local/bin)
install(DIRECTORY qml/ DESTINATION /usr/share/speeduino-ui/qml)

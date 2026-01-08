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
        qml/main.qml
        qml/Home.qml
        qml/Dash.qml
        qml/ReverseCamera.qml
        qml/Settings.qml
        qml/components/Gauge.qml
        qml/components/WarningIndicator.qml
        qml/components/IconButton.qml
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

# Install
install(TARGETS hmi_launcher RUNTIME DESTINATION /usr/local/bin)

add_executable(can_service
    src/main.cpp
    src/can_interface.cpp
    src/can_parser.cpp
    src/can_writer.cpp
    src/signal_database.cpp
    src/zmq_publisher.cpp
    src/can_error_handler.cpp
    src/obd_handler.cpp
    src/wideband_forwarder.cpp
    src/bmw_cluster_sim.cpp
)

target_include_directories(can_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(can_service PRIVATE
    common
    ${ZMQ_LIBRARIES}
    Threads::Threads
)

# Enable real-time capabilities (requires CAP_SYS_NICE)
target_compile_definitions(can_service PRIVATE
    ENABLE_REALTIME_SCHEDULING
)

# Install
install(TARGETS can_service RUNTIME DESTINATION /usr/local/bin)

# Unit tests
if(BUILD_TESTS AND GTest_FOUND)
    add_executable(test_can_parser
        tests/test_can_parser.cpp
        src/can_parser.cpp
        src/signal_database.cpp
    )
    target_include_directories(test_can_parser PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_can_parser PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME test_can_parser COMMAND test_can_parser)

    add_executable(test_can_error_handler
        tests/test_can_error_handler.cpp
        src/can_error_handler.cpp
    )
    target_include_directories(test_can_error_handler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_can_error_handler PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME test_can_error_handler COMMAND test_can_error_handler)

    # CAN Interface tests
    add_executable(test_can_interface
        tests/test_can_interface.cpp
        src/can_interface.cpp
    )
    target_include_directories(test_can_interface PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_can_interface PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )
    add_test(NAME test_can_interface COMMAND test_can_interface)

    # ZMQ Publisher tests - IPC communication
    add_executable(test_zmq_publisher
        tests/test_zmq_publisher.cpp
        src/zmq_publisher.cpp
    )
    target_include_directories(test_zmq_publisher PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ZMQ_INCLUDE_DIRS}
        ${MSGPACK_INCLUDE_DIR}
    )
    target_link_libraries(test_zmq_publisher PRIVATE
        common
        ${ZMQ_LIBRARIES}
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )
    add_test(NAME test_zmq_publisher COMMAND test_zmq_publisher)

    # OBD Handler tests
    add_executable(test_obd_handler
        tests/test_obd_handler.cpp
        src/obd_handler.cpp
        src/can_interface.cpp
    )
    target_include_directories(test_obd_handler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_obd_handler PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )
    add_test(NAME test_obd_handler COMMAND test_obd_handler)

    # Wideband Forwarder tests
    add_executable(test_wideband_forwarder
        tests/test_wideband_forwarder.cpp
        src/wideband_forwarder.cpp
        src/can_interface.cpp
    )
    target_include_directories(test_wideband_forwarder PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_wideband_forwarder PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )
    add_test(NAME test_wideband_forwarder COMMAND test_wideband_forwarder)

    # BMW Cluster Simulator tests
    add_executable(test_bmw_cluster_sim
        tests/test_bmw_cluster_sim.cpp
        src/bmw_cluster_sim.cpp
        src/can_interface.cpp
    )
    target_include_directories(test_bmw_cluster_sim PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_bmw_cluster_sim PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )
    add_test(NAME test_bmw_cluster_sim COMMAND test_bmw_cluster_sim)
endif()

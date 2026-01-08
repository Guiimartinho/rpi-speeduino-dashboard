add_executable(can_service
    src/main.cpp
    src/can_interface.cpp
    src/can_parser.cpp
    src/can_writer.cpp
    src/signal_database.cpp
    src/zmq_publisher.cpp
    src/can_error_handler.cpp
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
endif()

add_library(common STATIC
    src/config_loader.cpp
    src/logger.cpp
    src/system_health.cpp
    src/watchdog_notifier.cpp
    src/realtime_utils.cpp
    src/config_watcher.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${YAML_CPP_INCLUDE_DIRS}
    ${MSGPACK_INCLUDE_DIR}
)

# Base libraries
set(COMMON_LIBS
    ${YAML_CPP_LIBRARIES}
    ${ZMQ_LIBRARIES}
    Threads::Threads
)

# Add libsystemd if available (for watchdog support)
if(SYSTEMD_FOUND)
    list(APPEND COMMON_LIBS ${SYSTEMD_LIBRARIES})
    target_include_directories(common PUBLIC ${SYSTEMD_INCLUDE_DIRS})
endif()

target_link_libraries(common PUBLIC ${COMMON_LIBS})

target_compile_definitions(common PUBLIC
    PROJECT_VERSION="${PROJECT_VERSION}"
)

# Unit tests for common utilities
if(BUILD_TESTS AND GTest_FOUND)
    add_executable(test_expected
        tests/test_expected.cpp
    )
    target_include_directories(test_expected PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_expected PRIVATE
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME test_expected COMMAND test_expected)

    add_executable(test_scope_guard
        tests/test_scope_guard.cpp
    )
    target_include_directories(test_scope_guard PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_scope_guard PRIVATE
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME test_scope_guard COMMAND test_scope_guard)

    # ConfigLoader tests - thread-safe configuration loading
    add_executable(test_config_loader
        tests/test_config_loader.cpp
        src/config_loader.cpp
        src/logger.cpp
    )
    target_include_directories(test_config_loader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${YAML_CPP_INCLUDE_DIRS}
    )
    target_link_libraries(test_config_loader PRIVATE
        ${YAML_CPP_LIBRARIES}
        Threads::Threads
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME test_config_loader COMMAND test_config_loader)

    # WatchdogNotifier tests - systemd integration
    add_executable(test_watchdog_notifier
        tests/test_watchdog_notifier.cpp
        src/watchdog_notifier.cpp
        src/logger.cpp
    )
    target_include_directories(test_watchdog_notifier PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_watchdog_notifier PRIVATE
        Threads::Threads
        GTest::gtest
        GTest::gtest_main
    )
    if(SYSTEMD_FOUND)
        target_link_libraries(test_watchdog_notifier PRIVATE ${SYSTEMD_LIBRARIES})
        target_include_directories(test_watchdog_notifier PRIVATE ${SYSTEMD_INCLUDE_DIRS})
    endif()
    add_test(NAME test_watchdog_notifier COMMAND test_watchdog_notifier)
endif()

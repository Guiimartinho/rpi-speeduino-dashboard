add_executable(reverse_service
    src/main.cpp
    src/reverse_detector.cpp
)

target_include_directories(reverse_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(reverse_service PRIVATE
    common
    ${ZMQ_LIBRARIES}
    Threads::Threads
)

# libgpiod for GPIO fallback (optional)
pkg_check_modules(GPIOD libgpiod)
if(GPIOD_FOUND)
    target_compile_definitions(reverse_service PRIVATE HAS_GPIOD)
    target_include_directories(reverse_service PRIVATE ${GPIOD_INCLUDE_DIRS})
    target_link_libraries(reverse_service PRIVATE ${GPIOD_LIBRARIES})
endif()

# Install
install(TARGETS reverse_service RUNTIME DESTINATION /usr/local/bin)

# Unit tests
if(BUILD_TESTS AND GTest_FOUND)
    add_executable(test_reverse_detector
        tests/test_reverse_detector.cpp
        src/reverse_detector.cpp
    )
    target_include_directories(test_reverse_detector PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_reverse_detector PRIVATE
        common
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Link libgpiod if available
    if(GPIOD_FOUND)
        target_compile_definitions(test_reverse_detector PRIVATE HAS_GPIOD)
        target_include_directories(test_reverse_detector PRIVATE ${GPIOD_INCLUDE_DIRS})
        target_link_libraries(test_reverse_detector PRIVATE ${GPIOD_LIBRARIES})
    endif()

    add_test(NAME test_reverse_detector COMMAND test_reverse_detector)
endif()

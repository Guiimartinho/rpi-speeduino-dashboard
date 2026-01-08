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

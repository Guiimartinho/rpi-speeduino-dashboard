add_library(common STATIC
    src/config_loader.cpp
    src/logger.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${YAML_CPP_INCLUDE_DIRS}
    ${MSGPACK_INCLUDE_DIR}
)

target_link_libraries(common PUBLIC
    ${YAML_CPP_LIBRARIES}
    ${ZMQ_LIBRARIES}
)

target_compile_definitions(common PUBLIC
    PROJECT_VERSION="${PROJECT_VERSION}"
)

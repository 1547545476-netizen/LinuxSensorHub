if(NOT UNIX)
    message(FATAL_ERROR "Linux SensorHub userspace programs require a Unix-like system.")
endif()

set(SENSORHUB_COMMON_SOURCES
    src/common/logger.cpp
    src/config/config.cpp
    src/storage/log_writer.cpp
    src/device/sensor_device.cpp
    src/net/tcp_broadcaster.cpp
    src/reactor/epoll_loop.cpp
)

add_library(sensorhub_common STATIC ${SENSORHUB_COMMON_SOURCES})
target_include_directories(sensorhub_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_definitions(sensorhub_common PUBLIC _GNU_SOURCE)
target_link_libraries(sensorhub_common PUBLIC pthread)

add_executable(sensorhubd
    src/main.cpp
)
target_link_libraries(sensorhubd PRIVATE sensorhub_common)

add_executable(sensorctl
    tools/sensorctl.cpp
)
target_link_libraries(sensorctl PRIVATE sensorhub_common)

add_executable(sensor_inject
    tools/sensor_inject.cpp
)
target_link_libraries(sensor_inject PRIVATE sensorhub_common)

add_executable(sensor_subscribe
    tools/sensor_subscribe.cpp
)
target_link_libraries(sensor_subscribe PRIVATE sensorhub_common)

install(TARGETS sensorhubd sensorctl sensor_inject sensor_subscribe RUNTIME DESTINATION bin)

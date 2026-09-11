add_executable(sensorhub_unit
    unit/test_config_queue.cpp
)
target_link_libraries(sensorhub_unit PRIVATE sensorhub_common)
add_test(NAME sensorhub_unit COMMAND sensorhub_unit)

add_test(
    NAME sensorhub_integration_simulated_daemon
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration/simulated_daemon.sh
            $<TARGET_FILE:sensorhubd>
            $<TARGET_FILE:sensorctl>
            ${PROJECT_SOURCE_DIR}
)


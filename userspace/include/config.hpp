#ifndef SENSORHUB_CONFIG_HPP
#define SENSORHUB_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace sensorhub {

struct AppConfig {
    std::string device_path{"/dev/sensorhub0"};
    std::string log_file{"logs/sensorhub.log"};
    std::string control_socket{"run/sensorhub.sock"};
    std::string udp_export_host{"127.0.0.1"};
    uint16_t tcp_listen_port{9090};
    uint16_t udp_export_port{9091};
    std::size_t worker_threads{2};
    std::size_t log_rotate_bytes{1024 * 1024};
    std::size_t queue_capacity{1024};
    bool simulate_device{false};
    uint32_t sensor_interval_ms{500};
};

AppConfig load_config_file(const std::string& path);

} // namespace sensorhub

#endif

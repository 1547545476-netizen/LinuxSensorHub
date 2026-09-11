#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace sensorhub {

namespace {

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool to_bool(const std::string& value) {
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

uint16_t to_port(const std::string& value) {
    int port = std::stoi(value);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("invalid port: " + value);
    }
    return static_cast<uint16_t>(port);
}

} // namespace

AppConfig load_config_file(const std::string& path) {
    AppConfig config;
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open config file: " + path);
    }

    std::string line;
    std::size_t line_no = 0;
    while (std::getline(in, line)) {
        line_no++;
        auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) {
            throw std::runtime_error("invalid config line " + std::to_string(line_no));
        }

        auto key = trim(line.substr(0, pos));
        auto value = trim(line.substr(pos + 1));

        /*
         * 初学者提示：
         * 配置解析不用写得玄乎。先把 key=value 读稳定，
         * 后面再考虑 TOML/YAML 这样的第三方库。
         */
        if (key == "device_path") config.device_path = value;
        else if (key == "log_file") config.log_file = value;
        else if (key == "control_socket") config.control_socket = value;
        else if (key == "udp_export_host") config.udp_export_host = value;
        else if (key == "tcp_listen_port") config.tcp_listen_port = to_port(value);
        else if (key == "udp_export_port") config.udp_export_port = to_port(value);
        else if (key == "worker_threads") config.worker_threads = static_cast<std::size_t>(std::stoul(value));
        else if (key == "log_rotate_bytes") config.log_rotate_bytes = static_cast<std::size_t>(std::stoul(value));
        else if (key == "queue_capacity") config.queue_capacity = static_cast<std::size_t>(std::stoul(value));
        else if (key == "simulate_device") config.simulate_device = to_bool(value);
        else if (key == "sensor_interval_ms") config.sensor_interval_ms = static_cast<uint32_t>(std::stoul(value));
    }

    if (config.worker_threads == 0) {
        throw std::runtime_error("worker_threads must be greater than 0");
    }
    if (config.queue_capacity == 0) {
        throw std::runtime_error("queue_capacity must be greater than 0");
    }

    return config;
}

} // namespace sensorhub

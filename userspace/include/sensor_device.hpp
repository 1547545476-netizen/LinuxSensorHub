#ifndef SENSORHUB_SENSOR_DEVICE_HPP
#define SENSORHUB_SENSOR_DEVICE_HPP

#include <string>
#include <vector>

#include "fd.hpp"
#include "sensorhub_uapi.h"

namespace sensorhub {

class SensorDevice {
public:
    void open_device(const std::string& path);
    int fd() const { return fd_.get(); }

    std::vector<sensorhub_sample> read_available();
    sensorhub_stats get_stats() const;
    sensorhub_config get_config() const;
    void set_config(const sensorhub_config& config) const;

private:
    UniqueFd fd_;
};

class SimulatedSensorDevice {
public:
    void start(uint32_t interval_ms);
    int fd() const { return timer_fd_.get(); }
    std::vector<sensorhub_sample> read_available();

private:
    UniqueFd timer_fd_;
    uint64_t seq_{0};
};

} // namespace sensorhub

#endif


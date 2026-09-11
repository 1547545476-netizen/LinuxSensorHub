#include "sensor_device.hpp"

#include <chrono>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/timerfd.h>
#include <unistd.h>
#include <fcntl.h>

namespace sensorhub {

namespace {

std::runtime_error sys_error(const std::string& action) {
    return std::runtime_error(action + ": " + std::strerror(errno));
}

} // namespace

void SensorDevice::open_device(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        throw sys_error("open " + path);
    }
    fd_.reset(fd);
}

std::vector<sensorhub_sample> SensorDevice::read_available() {
    std::vector<sensorhub_sample> out;
    sensorhub_sample buffer[64];

    for (;;) {
        ssize_t n = ::read(fd_.get(), buffer, sizeof(buffer));
        if (n > 0) {
            auto count = static_cast<std::size_t>(n) / sizeof(sensorhub_sample);
            out.insert(out.end(), buffer, buffer + count);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0) {
            throw sys_error("read sensor device");
        }
        break;
    }

    return out;
}

sensorhub_stats SensorDevice::get_stats() const {
    sensorhub_stats stats{};
    if (ioctl(fd_.get(), SENSORHUB_IOC_GET_STATS, &stats) < 0) {
        throw sys_error("ioctl GET_STATS");
    }
    return stats;
}

sensorhub_config SensorDevice::get_config() const {
    sensorhub_config config{};
    if (ioctl(fd_.get(), SENSORHUB_IOC_GET_CONFIG, &config) < 0) {
        throw sys_error("ioctl GET_CONFIG");
    }
    return config;
}

void SensorDevice::set_config(const sensorhub_config& config) const {
    if (ioctl(fd_.get(), SENSORHUB_IOC_SET_CONFIG, &config) < 0) {
        throw sys_error("ioctl SET_CONFIG");
    }
}

void SimulatedSensorDevice::start(uint32_t interval_ms) {
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        throw sys_error("timerfd_create");
    }

    itimerspec spec{};
    spec.it_value.tv_sec = interval_ms / 1000;
    spec.it_value.tv_nsec = (interval_ms % 1000) * 1000 * 1000;
    spec.it_interval = spec.it_value;
    if (timerfd_settime(fd, 0, &spec, nullptr) < 0) {
        ::close(fd);
        throw sys_error("timerfd_settime");
    }

    timer_fd_.reset(fd);
}

std::vector<sensorhub_sample> SimulatedSensorDevice::read_available() {
    uint64_t expirations = 0;
    ssize_t n = ::read(timer_fd_.get(), &expirations, sizeof(expirations));
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return {};
    }
    if (n < 0) {
        throw sys_error("read timerfd");
    }

    std::vector<sensorhub_sample> out;
    for (uint64_t i = 0; i < expirations; ++i) {
        sensorhub_sample sample{};
        sample.seq = seq_++;
        sample.timestamp_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        sample.type = SENSORHUB_SAMPLE_TEMP;
        sample.value0 = 25000 + static_cast<int32_t>((sample.seq * 37) % 8000);
        out.push_back(sample);
    }
    return out;
}

} // namespace sensorhub

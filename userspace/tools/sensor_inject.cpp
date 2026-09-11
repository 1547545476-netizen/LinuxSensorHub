#include "sensorhub_uapi.h"

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

std::runtime_error sys_error(const std::string& action) {
    return std::runtime_error(action + ": " + std::strerror(errno));
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string device = argc > 1 ? argv[1] : "/dev/sensorhub0";
        int value = argc > 2 ? std::stoi(argv[2]) : 12345;

        int fd = open(device.c_str(), O_WRONLY | O_CLOEXEC);
        if (fd < 0) {
            throw sys_error("open " + device);
        }

        sensorhub_sample sample{};
        sample.timestamp_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        sample.type = SENSORHUB_SAMPLE_USER;
        sample.value0 = value;

        ssize_t n = write(fd, &sample, sizeof(sample));
        close(fd);

        if (n != static_cast<ssize_t>(sizeof(sample))) {
            throw sys_error("write sample");
        }

        std::cout << "injected value=" << value << " to " << device << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "sensor_inject: " << ex.what() << '\n';
        return 1;
    }
}


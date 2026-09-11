#ifndef SENSORHUB_LOG_WRITER_HPP
#define SENSORHUB_LOG_WRITER_HPP

#include <cstddef>
#include <fstream>
#include <string>

#include "sensorhub_uapi.h"

namespace sensorhub {

class LogWriter {
public:
    LogWriter(std::string path, std::size_t rotate_bytes);

    void write_sample(const sensorhub_sample& sample);

private:
    void open_if_needed();
    void rotate_if_needed();

    std::string path_;
    std::size_t rotate_bytes_;
    std::ofstream stream_;
    std::size_t current_bytes_{0};
};

std::string sample_to_line(const sensorhub_sample& sample);

} // namespace sensorhub

#endif


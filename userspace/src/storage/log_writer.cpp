#include "log_writer.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace sensorhub {

namespace {

std::string sample_type_name(uint32_t type) {
    switch (type) {
    case SENSORHUB_SAMPLE_TEMP: return "temp";
    case SENSORHUB_SAMPLE_ACCEL: return "accel";
    case SENSORHUB_SAMPLE_GYRO: return "gyro";
    case SENSORHUB_SAMPLE_USER: return "user";
    default: return "unknown";
    }
}

} // namespace

LogWriter::LogWriter(std::string path, std::size_t rotate_bytes)
    : path_(std::move(path)), rotate_bytes_(rotate_bytes) {}

void LogWriter::write_sample(const sensorhub_sample& sample) {
    open_if_needed();
    rotate_if_needed();

    auto line = sample_to_line(sample);
    stream_ << line << '\n';
    stream_.flush();
    current_bytes_ += line.size() + 1;
}

void LogWriter::open_if_needed() {
    if (stream_.is_open()) {
        return;
    }

    std::filesystem::path file_path(path_);
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }

    current_bytes_ = std::filesystem::exists(file_path)
        ? static_cast<std::size_t>(std::filesystem::file_size(file_path))
        : 0;

    stream_.open(path_, std::ios::app);
    if (!stream_) {
        throw std::runtime_error("cannot open log file: " + path_);
    }
}

void LogWriter::rotate_if_needed() {
    if (rotate_bytes_ == 0 || current_bytes_ < rotate_bytes_) {
        return;
    }

    stream_.close();
    auto rotated = path_ + ".1";
    std::error_code ec;
    std::filesystem::remove(rotated, ec);
    std::filesystem::rename(path_, rotated, ec);
    current_bytes_ = 0;
    stream_.open(path_, std::ios::trunc);
    if (!stream_) {
        throw std::runtime_error("cannot reopen log file after rotation: " + path_);
    }
}

std::string sample_to_line(const sensorhub_sample& sample) {
    std::ostringstream oss;
    oss << "seq=" << sample.seq
        << " ts_ns=" << sample.timestamp_ns
        << " type=" << sample_type_name(sample.type)
        << " v0=" << sample.value0
        << " v1=" << sample.value1
        << " v2=" << sample.value2;
    return oss.str();
}

} // namespace sensorhub

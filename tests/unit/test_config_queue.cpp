#include "bounded_queue.hpp"
#include "config.hpp"
#include "log_writer.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace sensorhub;

namespace {

std::filesystem::path make_temp_dir() {
    auto dir = std::filesystem::temp_directory_path() / "sensorhub_unit_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void test_config() {
    auto dir = make_temp_dir();
    auto path = dir / "sensorhub.conf";
    std::ofstream out(path);
    out << "device_path=/tmp/fake\n";
    out << "log_file=/tmp/sensorhub.log\n";
    out << "control_socket=/tmp/sensorhub.sock\n";
    out << "worker_threads=3\n";
    out << "queue_capacity=7\n";
    out << "simulate_device=true\n";
    out << "sensor_interval_ms=50\n";
    out.close();

    auto cfg = load_config_file(path.string());
    assert(cfg.device_path == "/tmp/fake");
    assert(cfg.worker_threads == 3);
    assert(cfg.queue_capacity == 7);
    assert(cfg.simulate_device);
    assert(cfg.sensor_interval_ms == 50);
}

void test_bounded_queue() {
    BoundedQueue<int> queue(2);
    assert(queue.push(1));
    assert(queue.push(2));
    assert(!queue.push(3));
    assert(queue.dropped() == 1);

    auto first = queue.pop();
    auto second = queue.pop();
    assert(first.has_value() && *first == 1);
    assert(second.has_value() && *second == 2);
    queue.close();
    assert(!queue.pop().has_value());
}

void test_log_writer() {
    auto dir = make_temp_dir();
    auto path = dir / "sensorhub.log";
    LogWriter writer(path.string(), 80);

    sensorhub_sample sample{};
    sample.seq = 1;
    sample.timestamp_ns = 2;
    sample.type = SENSORHUB_SAMPLE_TEMP;
    sample.value0 = 25000;

    writer.write_sample(sample);
    writer.write_sample(sample);
    writer.write_sample(sample);

    assert(std::filesystem::exists(path));
    assert(std::filesystem::exists(path.string() + ".1"));
}

} // namespace

int main() {
    test_config();
    test_bounded_queue();
    test_log_writer();
    std::cout << "sensorhub_unit passed\n";
    return 0;
}


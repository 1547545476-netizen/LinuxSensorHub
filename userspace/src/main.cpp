#include "bounded_queue.hpp"
#include "config.hpp"
#include "control_server.hpp"
#include "epoll_loop.hpp"
#include "fd.hpp"
#include "log_writer.hpp"
#include "logger.hpp"
#include "sensor_device.hpp"
#include "tcp_broadcaster.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace sensorhub {

namespace {

struct RuntimeStats {
    std::atomic<uint64_t> received{0};
    std::atomic<uint64_t> processed{0};
    std::atomic<uint64_t> exported{0};
};

std::runtime_error sys_error(const std::string& action) {
    return std::runtime_error(action + ": " + std::strerror(errno));
}

UniqueFd create_signal_fd() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    /*
     * 初学者提示：
     * 普通 signal handler 里能做的事情很少。
     * signalfd 把信号变成 fd，于是可以统一交给 epoll 管理。
     */
    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) != 0) {
        throw sys_error("pthread_sigmask");
    }

    int fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd < 0) {
        throw sys_error("signalfd");
    }
    return UniqueFd(fd);
}

UniqueFd create_udp_socket(const AppConfig& config, sockaddr_in& addr) {
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        throw sys_error("socket UDP");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.udp_export_port);
    if (inet_pton(AF_INET, config.udp_export_host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        throw std::runtime_error("invalid udp_export_host: " + config.udp_export_host);
    }

    return UniqueFd(fd);
}

UniqueFd create_control_socket(const std::string& path) {
    std::filesystem::path sock_path(path);
    if (sock_path.has_parent_path()) {
        std::filesystem::create_directories(sock_path.parent_path());
    }

    unlink(path.c_str());

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        throw sys_error("socket AF_UNIX");
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        close(fd);
        throw std::runtime_error("control socket path is too long");
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        throw sys_error("bind control socket");
    }

    if (listen(fd, 16) < 0) {
        close(fd);
        throw sys_error("listen control socket");
    }

    return UniqueFd(fd);
}

std::string build_stats_text(const RuntimeStats& stats,
                             const BoundedQueue<sensorhub_sample>& queue,
                             const TcpBroadcaster& tcp,
                             bool simulate,
                             const SensorDevice* device)
{
    std::string text;
    text += "mode=" + std::string(simulate ? "simulate" : "driver") + "\n";
    text += "received=" + std::to_string(stats.received.load()) + "\n";
    text += "processed=" + std::to_string(stats.processed.load()) + "\n";
    text += "exported=" + std::to_string(stats.exported.load()) + "\n";
    text += "tcp_clients=" + std::to_string(tcp.client_count()) + "\n";
    text += "queue_size=" + std::to_string(queue.size()) + "\n";
    text += "queue_dropped=" + std::to_string(queue.dropped()) + "\n";

    if (!simulate && device) {
        try {
            auto dev_stats = device->get_stats();
            text += "driver_generated=" + std::to_string(dev_stats.generated) + "\n";
            text += "driver_injected=" + std::to_string(dev_stats.injected) + "\n";
            text += "driver_dropped=" + std::to_string(dev_stats.dropped) + "\n";
            text += "driver_fifo_bytes=" + std::to_string(dev_stats.fifo_bytes) + "\n";
        } catch (const std::exception& ex) {
            text += "driver_error=" + std::string(ex.what()) + "\n";
        }
    }

    return text;
}

std::string handle_control_command(std::string cmd,
                           const std::string& config_path,
                           RuntimeStats& stats,
                           BoundedQueue<sensorhub_sample>& queue,
                           TcpBroadcaster& tcp,
                           bool& running,
                           bool simulate,
                           SensorDevice* device)
{
    while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' ')) {
        cmd.pop_back();
    }

    std::string reply;
    if (cmd == "stats") {
        reply = build_stats_text(stats, queue, tcp, simulate, device);
    } else if (cmd == "reload") {
        auto reloaded = load_config_file(config_path);
        if (!simulate && device) {
            sensorhub_config drv_cfg{};
            drv_cfg.interval_ms = reloaded.sensor_interval_ms;
            drv_cfg.enabled = 1;
            device->set_config(drv_cfg);
        }
        reply = "ok\n";
    } else if (cmd == "shutdown") {
        running = false;
        reply = "ok\n";
    } else {
        reply = "unknown command\n";
    }

    return reply;
}

std::string parse_config_arg(int argc, char** argv) {
    std::string config_path = "config/sensorhub.conf";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "usage: sensorhubd [-c config/sensorhub.conf]\n";
            std::exit(0);
        }
    }
    return config_path;
}

} // namespace

} // namespace sensorhub

int main(int argc, char** argv) {
    using namespace sensorhub;

    try {
        auto config_path = parse_config_arg(argc, argv);
        auto config = load_config_file(config_path);

        log_info("starting sensorhubd");

        RuntimeStats stats;
        BoundedQueue<sensorhub_sample> queue(config.queue_capacity);
        std::mutex writer_mutex;
        LogWriter writer(config.log_file, config.log_rotate_bytes);

        sockaddr_in udp_addr{};
        auto udp_fd = create_udp_socket(config, udp_addr);
        TcpBroadcaster tcp;
        tcp.listen_on(config.tcp_listen_port);

        /*
         * 初学者提示：
         * 主线程只负责收事件和投递任务，耗时的落盘和网络发送交给 worker。
         * 这样设备 fd 上有数据时，epoll 主循环能尽快回来继续处理事件。
         */
        std::vector<std::thread> workers;
        for (std::size_t i = 0; i < config.worker_threads; ++i) {
            workers.emplace_back([&] {
                for (;;) {
                    auto item = queue.pop();
                    if (!item.has_value()) {
                        break;
                    }
                    auto line = sample_to_line(*item);
                    {
                        /*
                         * 初学者提示：
                         * 多个 worker 可以并发解析数据，但同一个文件最好串行写。
                         * mutex 把“共享文件”保护起来，避免日志行互相穿插。
                         */
                        std::lock_guard<std::mutex> lock(writer_mutex);
                        writer.write_sample(*item);
                    }
                    sendto(udp_fd.get(), line.data(), line.size(), MSG_NOSIGNAL,
                           reinterpret_cast<sockaddr*>(&udp_addr), sizeof(udp_addr));
                    tcp.broadcast_line(line);
                    stats.processed++;
                    stats.exported++;
                }
            });
        }

        SensorDevice real_device;
        SimulatedSensorDevice simulated_device;
        bool simulate = config.simulate_device;
        int source_fd = -1;
        std::function<std::vector<sensorhub_sample>()> read_samples;

        if (simulate) {
            simulated_device.start(config.sensor_interval_ms);
            source_fd = simulated_device.fd();
            read_samples = [&] { return simulated_device.read_available(); };
            log_info("using simulated timerfd device");
        } else {
            real_device.open_device(config.device_path);
            source_fd = real_device.fd();
            read_samples = [&] { return real_device.read_available(); };
            log_info("using kernel driver device: " + config.device_path);
        }

        auto signal_fd = create_signal_fd();
        auto control_fd = create_control_socket(config.control_socket);

        bool running = true;
        EpollLoop loop;
        // 连接到达和命令到达是两个事件，ControlServer 保留连接直到收齐一行。
        ControlServer control(loop, [&](std::string cmd) {
            return handle_control_command(std::move(cmd), config_path, stats, queue,
                                          tcp, running, simulate,
                                          simulate ? nullptr : &real_device);
        });

        loop.add(source_fd, EPOLLIN, [&](uint32_t) {
            for (const auto& sample : read_samples()) {
                stats.received++;
                queue.push(sample);
            }
        });

        loop.add(signal_fd.get(), EPOLLIN, [&](uint32_t) {
            signalfd_siginfo info{};
            read(signal_fd.get(), &info, sizeof(info));
            log_info("received signal, shutting down");
            running = false;
        });

        loop.add(control_fd.get(), EPOLLIN, [&](uint32_t) {
            control.accept_ready(control_fd.get());
        });

        loop.add(tcp.listen_fd(), EPOLLIN, [&](uint32_t) {
            tcp.accept_ready();
        });

        log_info("sensorhubd is ready");
        while (running) {
            loop.run_once(1000);
        }

        queue.close();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        unlink(config.control_socket.c_str());
        log_info("sensorhubd stopped");
        return 0;
    } catch (const std::exception& ex) {
        sensorhub::log_error(ex.what());
        return 1;
    }
}

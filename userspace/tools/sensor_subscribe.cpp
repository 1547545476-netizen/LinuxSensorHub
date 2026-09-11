#include "config.hpp"
#include "fd.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

std::runtime_error sys_error(const std::string& action) {
    return std::runtime_error(action + ": " + std::strerror(errno));
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string config_path = "config/sensorhub.conf";
        int count = 5;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
                config_path = argv[++i];
            } else if ((arg == "-n" || arg == "--count") && i + 1 < argc) {
                count = std::stoi(argv[++i]);
            }
        }

        auto config = sensorhub::load_config_file(config_path);

        sensorhub::UniqueFd fd(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
        if (!fd.valid()) {
            throw sys_error("socket");
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config.tcp_listen_port);
        if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
            throw std::runtime_error("inet_pton failed");
        }

        if (connect(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw sys_error("connect TCP broadcaster");
        }

        std::string pending;
        char buf[1024];
        int lines = 0;
        while (lines < count) {
            ssize_t n = read(fd.get(), buf, sizeof(buf));
            if (n < 0) {
                throw sys_error("read TCP broadcaster");
            }
            if (n == 0) {
                break;
            }
            pending.append(buf, static_cast<std::size_t>(n));
            std::size_t pos;
            while ((pos = pending.find('\n')) != std::string::npos && lines < count) {
                std::cout << pending.substr(0, pos) << '\n';
                pending.erase(0, pos + 1);
                lines++;
            }
        }

        return lines == count ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "sensor_subscribe: " << ex.what() << '\n';
        return 1;
    }
}


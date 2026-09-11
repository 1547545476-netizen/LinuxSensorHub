#include "config.hpp"
#include "fd.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

std::runtime_error sys_error(const std::string& action) {
    return std::runtime_error(action + ": " + std::strerror(errno));
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string config_path = "config/sensorhub.conf";
        std::string command = argc > 1 ? argv[1] : "stats";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg != "stats" && arg != "reload" && arg != "shutdown") {
                command = arg;
            }
        }

        auto config = sensorhub::load_config_file(config_path);

        sensorhub::UniqueFd fd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
        if (!fd.valid()) {
            throw sys_error("socket");
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (config.control_socket.size() >= sizeof(addr.sun_path)) {
            throw std::runtime_error("control socket path is too long");
        }
        std::strncpy(addr.sun_path, config.control_socket.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw sys_error("connect " + config.control_socket);
        }

        command += "\n";
        write(fd.get(), command.data(), command.size());

        char buf[4096];
        ssize_t n;
        while ((n = read(fd.get(), buf, sizeof(buf))) > 0) {
            std::cout.write(buf, n);
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "sensorctl: " << ex.what() << '\n';
        return 1;
    }
}


#include "tcp_broadcaster.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace sensorhub {

namespace {

std::runtime_error sys_error(const std::string& action) {
    return std::runtime_error(action + ": " + std::strerror(errno));
}

} // namespace

void TcpBroadcaster::listen_on(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        throw sys_error("socket TCP");
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        throw sys_error("bind TCP");
    }

    if (listen(fd, 64) < 0) {
        close(fd);
        throw sys_error("listen TCP");
    }

    listen_fd_.reset(fd);
}

void TcpBroadcaster::accept_ready() {
    for (;;) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int fd = accept4(listen_fd_.get(), reinterpret_cast<sockaddr*>(&client_addr),
                         &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            throw sys_error("accept TCP client");
        }

        /*
         * 初学者提示：
         * 这里不为每个 TCP 客户端创建线程。服务只保存客户端 fd，
         * worker 线程在有新数据时把一行文本广播给所有订阅者。
         */
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.emplace_back(fd);
    }
}

void TcpBroadcaster::broadcast_line(const std::string& line) {
    std::string payload = line + "\n";
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = clients_.begin(); it != clients_.end();) {
        ssize_t n = send(it->get(), payload.data(), payload.size(), MSG_NOSIGNAL);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ++it;
            continue;
        }
        if (n < 0 || static_cast<std::size_t>(n) != payload.size()) {
            it = clients_.erase(it);
            continue;
        }
        ++it;
    }
}

std::size_t TcpBroadcaster::client_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_.size();
}

} // namespace sensorhub


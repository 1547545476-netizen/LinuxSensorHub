#include "control_server.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <utility>

namespace sensorhub {

ControlServer::ControlServer(EpollLoop& loop, Handler handler)
    : loop_(loop), handler_(std::move(handler)) {}

ControlServer::~ControlServer() {
    // 先从 epoll 注销，再由 UniqueFd 关闭连接，防止残留回调访问已销毁对象。
    for (const auto& entry : clients_) {
        loop_.remove(entry.first);
    }
}

void ControlServer::accept_ready(int listen_fd) {
    for (;;) {
        UniqueFd fd(accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
        if (!fd.valid()) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            throw std::runtime_error(std::string("accept control client: ") + std::strerror(errno));
        }
        add_client(std::move(fd));
    }
}

void ControlServer::add_client(UniqueFd fd) {
    // 限制未完成连接数；慢客户端不能让本地控制接口无限消耗文件描述符。
    if (clients_.size() >= 64) return;
    const int key = fd.get();
    clients_.emplace(key, Client{std::move(fd), {}, {}, 0, false});
    try {
        loop_.add(key, EPOLLIN | EPOLLRDHUP, [this, key](uint32_t events) {
            ready(key, events);
        });
    } catch (...) {
        clients_.erase(key);
        throw;
    }
}

void ControlServer::close_client(int fd) {
    loop_.remove(fd);
    clients_.erase(fd);
}

void ControlServer::ready(int fd, uint32_t events) {
    auto& client = clients_.at(fd);
    if (!client.responding) {
        char buffer[128];
        for (;;) {
            const ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                // 暂时没数据不是断开连接，保留缓冲区，下一次 EPOLLIN 再读。
                if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
                    !(events & (EPOLLERR | EPOLLHUP))) return;
                close_client(fd);
                return;
            }
            if (n == 0) {
                close_client(fd);
                return;
            }
            client.request.append(buffer, static_cast<std::size_t>(n));
            const auto newline = client.request.find('\n');
            if (newline != std::string::npos && newline <= 127) {
                try {
                    client.response = handler_(client.request.substr(0, newline));
                } catch (const std::exception&) {
                    client.response = "error: command failed\n";
                }
                client.responding = true;
                break;
            }
            if (client.request.size() > 127) {
                client.response = "error: command too long\n";
                client.responding = true;
                break;
            }
        }
    }

    // STREAM Socket 可能短写；保留发送偏移，EAGAIN 时只等待可写事件，避免忙循环。
    while (client.sent < client.response.size()) {
        const ssize_t n = send(fd, client.response.data() + client.sent,
                               client.response.size() - client.sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
                !(events & (EPOLLERR | EPOLLHUP))) {
                loop_.modify(fd, EPOLLOUT);
                return;
            }
            close_client(fd);
            return;
        }
        if (n == 0) {
            close_client(fd);
            return;
        }
        client.sent += static_cast<std::size_t>(n);
    }
    close_client(fd);
}

} // namespace sensorhub

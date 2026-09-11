#include "epoll_loop.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>

namespace sensorhub {

namespace {

std::runtime_error sys_error(const std::string& action) {
    return std::runtime_error(action + ": " + std::strerror(errno));
}

} // namespace

EpollLoop::EpollLoop() {
    int fd = epoll_create1(EPOLL_CLOEXEC);
    if (fd < 0) {
        throw sys_error("epoll_create1");
    }
    epoll_fd_.reset(fd);
}

void EpollLoop::add(int fd, uint32_t events, Callback cb) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw sys_error("epoll_ctl ADD");
    }
    callbacks_[fd] = std::make_shared<Callback>(std::move(cb));
}

void EpollLoop::modify(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_MOD, fd, &ev) < 0) {
        throw sys_error("epoll_ctl MOD");
    }
}

void EpollLoop::remove(int fd) {
    epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr);
    callbacks_.erase(fd);
}

void EpollLoop::run_once(int timeout_ms) {
    epoll_event events[32];
    int n = epoll_wait(epoll_fd_.get(), events, 32, timeout_ms);
    if (n < 0) {
        if (errno == EINTR) {
            return;
        }
        throw sys_error("epoll_wait");
    }

    for (int i = 0; i < n; ++i) {
        auto it = callbacks_.find(events[i].data.fd);
        if (it != callbacks_.end()) {
            // 回调可能注销自己；局部共享所有权保证它执行结束前不会被销毁。
            // 不复制函数本体，避免有状态的 lambda 每次调用都丢失内部状态。
            auto callback = it->second;
            (*callback)(events[i].events);
        }
    }
}

} // namespace sensorhub

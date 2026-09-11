#ifndef SENSORHUB_EPOLL_LOOP_HPP
#define SENSORHUB_EPOLL_LOOP_HPP

#include <functional>
#include <sys/epoll.h>
#include <unordered_map>

#include "fd.hpp"

namespace sensorhub {

class EpollLoop {
public:
    using Callback = std::function<void(uint32_t events)>;

    EpollLoop();
    void add(int fd, uint32_t events, Callback cb);
    void remove(int fd);
    void run_once(int timeout_ms);

private:
    UniqueFd epoll_fd_;
    std::unordered_map<int, Callback> callbacks_;
};

} // namespace sensorhub

#endif


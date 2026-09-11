#ifndef SENSORHUB_TCP_BROADCASTER_HPP
#define SENSORHUB_TCP_BROADCASTER_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "fd.hpp"

namespace sensorhub {

class TcpBroadcaster {
public:
    void listen_on(uint16_t port);
    int listen_fd() const { return listen_fd_.get(); }
    void accept_ready();
    void broadcast_line(const std::string& line);
    std::size_t client_count() const;

private:
    UniqueFd listen_fd_;
    mutable std::mutex mutex_;
    std::vector<UniqueFd> clients_;
};

} // namespace sensorhub

#endif


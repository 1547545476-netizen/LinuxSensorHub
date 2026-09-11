#ifndef SENSORHUB_CONTROL_SERVER_HPP
#define SENSORHUB_CONTROL_SERVER_HPP

#include "epoll_loop.hpp"
#include "fd.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace sensorhub {

// 每个控制连接只处理一条以换行结尾的命令；所有状态只由 epoll 主线程访问。
class ControlServer {
public:
    using Handler = std::function<std::string(std::string)>;
    ControlServer(EpollLoop& loop, Handler handler);
    ~ControlServer();
    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;

    void accept_ready(int listen_fd);
    // 接管一个非阻塞连接；测试也可通过 socketpair 注入连接。
    void add_client(UniqueFd fd);

private:
    struct Client {
        UniqueFd fd;
        std::string request;
        std::string response;
        std::size_t sent = 0;
        bool responding = false;
    };
    void ready(int fd, uint32_t events);
    void close_client(int fd);

    EpollLoop& loop_;
    Handler handler_;
    std::unordered_map<int, Client> clients_;
};

} // namespace sensorhub
#endif

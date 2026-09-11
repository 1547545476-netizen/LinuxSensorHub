#include "control_server.hpp"

#include <cassert>
#include <cerrno>
#include <string>
#include <sys/socket.h>
#include <utility>

using namespace sensorhub;

namespace {

UniqueFd connect_pair(ControlServer& server) {
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, pair) == 0);
    UniqueFd client(pair[0]);
    int size = 1024;
    assert(setsockopt(pair[1], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == 0);
    server.add_client(UniqueFd(pair[1]));
    return client;
}

void send_text(int fd, const std::string& text) {
    assert(send(fd, text.data(), text.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(text.size()));
}

std::string drain(EpollLoop& loop, int fd) {
    std::string result;
    char buffer[4096];
    for (int attempt = 0; attempt < 2000; ++attempt) {
        loop.run_once(1);
        for (;;) {
            const ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
            if (n > 0) result.append(buffer, static_cast<std::size_t>(n));
            else if (n == 0) return result;
            else {
                assert(errno == EAGAIN || errno == EWOULDBLOCK);
                break;
            }
        }
    }
    assert(false && "response timed out");
    return result;
}

} // namespace

int main() {
    EpollLoop loop;
    int calls = 0;
    ControlServer server(loop, [&](std::string command) {
        ++calls;
        if (command == "large") return std::string(65536, 'x');
        return "ok:" + command + "\n";
    });

    // 已连接但尚未发送，以及一条命令分两次发送，都不能被当成断开。
    auto delayed = connect_pair(server);
    loop.run_once(1);
    assert(calls == 0);
    send_text(delayed.get(), "sta");
    loop.run_once(1);
    assert(calls == 0);

    // 一个慢连接不应阻塞另一个正常控制请求。
    auto fast = connect_pair(server);
    send_text(fast.get(), "stats\n");
    assert(drain(loop, fast.get()) == "ok:stats\n");
    send_text(delayed.get(), "ts\n");
    assert(drain(loop, delayed.get()) == "ok:stats\n");
    assert(calls == 2);

    // 小发送缓冲区强制覆盖短写/EAGAIN，最终内容必须完整且只执行一次命令。
    auto large = connect_pair(server);
    send_text(large.get(), "large\n");
    loop.run_once(1);
    assert(calls == 3);
    assert(drain(loop, large.get()) == std::string(65536, 'x'));
    assert(calls == 3);

    auto oversized = connect_pair(server);
    send_text(oversized.get(), std::string(128, 'a'));
    assert(drain(loop, oversized.get()) == "error: command too long\n");
    assert(calls == 3);

    auto half_closed = connect_pair(server);
    send_text(half_closed.get(), "stats\n");
    assert(shutdown(half_closed.get(), SHUT_WR) == 0);
    assert(drain(loop, half_closed.get()) == "ok:stats\n");

    // 对端提前关闭，服务仍能继续处理后续请求，不因 SIGPIPE 退出。
    auto gone = connect_pair(server);
    send_text(gone.get(), "stats\n");
    gone.reset();
    loop.run_once(1);
    auto last = connect_pair(server);
    send_text(last.get(), "stats\n");
    assert(drain(loop, last.get()) == "ok:stats\n");
}

#ifndef SENSORHUB_FD_HPP
#define SENSORHUB_FD_HPP

#include <unistd.h>

namespace sensorhub {

class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() { reset(); }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const { return fd_; }
    bool valid() const { return fd_ >= 0; }

    int release() {
        int old = fd_;
        fd_ = -1;
        return old;
    }

    void reset(int next = -1) {
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = next;
    }

private:
    int fd_{-1};
};

} // namespace sensorhub

#endif


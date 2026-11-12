#include "common.hpp"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

int parse_integer(const char* text, int minimum, const char* name) {
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);

    if (errno != 0 || text == end || *end != '\0' ||
        value < minimum || value > INT_MAX) {
        throw std::runtime_error(std::string("Invalid ") + name + ": " + text);
    }

    return static_cast<int>(value);
}

[[noreturn]] void posix_error(const char* operation, int code) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(code));
}

[[noreturn]] void fatal_posix(const char* operation, int code) noexcept {
    char buffer[512];
    int count = std::snprintf(buffer, sizeof buffer, "FATAL: %s: error=%d\n", operation, code);

    if (count > 0 && static_cast<std::size_t>(count) < sizeof buffer) {
        std::size_t done = 0;
        while (done < static_cast<std::size_t>(count)) {
            const ssize_t n = ::write(STDERR_FILENO, buffer + done,
                                      static_cast<std::size_t>(count) - done);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            done += static_cast<std::size_t>(n);
        }
    }
    std::_Exit(EXIT_FAILURE);
}

void write_stdout(const char* text, std::size_t count) noexcept {
    std::size_t done = 0;
    while (done < count) {
        const ssize_t n = ::write(STDOUT_FILENO, text + done, count - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) fatal_posix("write(stdout)", n < 0 ? errno : EIO);
        done += static_cast<std::size_t>(n);
    }
}

double monotonic_seconds() {
    timespec ts{};
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        posix_error("clock_gettime", errno);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

long linux_tid() noexcept {
    const long tid = ::syscall(SYS_gettid);
    if (tid == -1) fatal_posix("gettid", errno);
    return tid;
}

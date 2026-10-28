#include "os.h"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>

struct os::FileHandleDummy {
    int fd = -1;
    ~FileHandleDummy() { if (fd >= 0) ::close(fd); }
};
struct os::ProcessHandleDummy {
    pid_t pid = -1;
    bool waited = false;
    int result = 0;
};

namespace {
[[noreturn]] void Fail(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
int Descriptor(os::FileHandle file) {
    if (!file) throw std::invalid_argument("Null file handle");
    return file->fd;
}
// Called only after fork: use async-signal-safe operations, not C++ streams.
[[noreturn]] void ChildFail(int reportFd, int error) {
    const char* data = reinterpret_cast<const char*>(&error);
    std::size_t remaining = sizeof(error);
    while (remaining > 0) {
        const ssize_t n = ::write(reportFd, data, remaining);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        data += n;
        remaining -= static_cast<std::size_t>(n);
    }
    _exit(127);
}
void Redirect(os::FileHandle file, int target, int errorFd) {
    if (!file) return;
    if (file->fd == target) {
        if (::fcntl(target, F_SETFD, 0) < 0) ChildFail(errorFd, errno);
    } else if (::dup2(file->fd, target) < 0) {
        ChildFail(errorFd, errno);
    }
}
}

bool os::CreatePipe(FileHandle& readPipe, FileHandle& writePipe) {
    auto read = std::make_unique<FileHandleDummy>();
    auto write = std::make_unique<FileHandleDummy>();
    int descriptors[2];
    if (::pipe2(descriptors, O_CLOEXEC) < 0) Fail("pipe2");
    read->fd = descriptors[0];
    write->fd = descriptors[1];
    readPipe = read.release();
    writePipe = write.release();
    return true;
}

void os::SetInheritable(FileHandle file, bool inherit) {
    const int fd = Descriptor(file);
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags < 0 || ::fcntl(fd, F_SETFD, inherit ? flags & ~FD_CLOEXEC : flags | FD_CLOEXEC) < 0)
        Fail("fcntl(FD_CLOEXEC)");
}

os::FileHandle os::OpenWrite(const std::string& filename) {
    auto file = std::make_unique<FileHandleDummy>();
    file->fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (file->fd < 0) Fail("open(output file)");
    return file.release();
}

void os::CloseFile(FileHandle file) noexcept { delete file; }

int os::PipeRead(FileHandle file, void* buffer, int count) {
    if (count < 0) throw std::invalid_argument("Negative read size");
    ssize_t result;
    do { result = ::read(Descriptor(file), buffer, static_cast<std::size_t>(count)); }
    while (result < 0 && errno == EINTR);
    if (result < 0) Fail("read");
    return static_cast<int>(result);
}
int os::PipeWrite(FileHandle file, const void* buffer, int count) {
    if (count < 0) throw std::invalid_argument("Negative write size");
    ssize_t result;
    do { result = ::write(Descriptor(file), buffer, static_cast<std::size_t>(count)); }
    while (result < 0 && errno == EINTR);
    if (result < 0) Fail("write");
    return static_cast<int>(result);
}

os::ProcessHandle os::CreateProc(const ProcessStartInfo& info) {
    if (info.executeCmdAsShell) throw std::invalid_argument("Shell execution is disabled");
    auto process = std::make_unique<ProcessHandleDummy>();
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(info.cmd.c_str()));
    for (const auto& arg : info.args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    FileHandle readRaw = nullptr, writeRaw = nullptr;
    CreatePipe(readRaw, writeRaw);
    std::unique_ptr<FileHandleDummy> errorRead(readRaw), errorWrite(writeRaw);
    const pid_t pid = ::fork();
    if (pid < 0) Fail("fork");
    if (pid == 0) {
        ::close(errorRead->fd);
        const int report = errorWrite->fd;
        Redirect(info.redirectStdin, STDIN_FILENO, report);
        Redirect(info.redirectStdout, STDOUT_FILENO, report);
        Redirect(info.redirectStderr, STDERR_FILENO, report);
        if (!info.workDir.empty() && ::chdir(info.workDir.c_str()) < 0) ChildFail(report, errno);
        // All original pipe ends have FD_CLOEXEC; only redirected stdio survives exec.
        ::execv(info.cmd.c_str(), argv.data());
        ChildFail(report, errno);
    }
    errorWrite.reset();
    int childError = 0;
    int received = 0;
    try {
        while (received < static_cast<int>(sizeof(childError))) {
            const int n = PipeRead(errorRead.get(), reinterpret_cast<char*>(&childError) + received,
                                   static_cast<int>(sizeof(childError)) - received);
            if (n == 0) break; // CLOEXEC closed the report pipe: exec succeeded.
            received += n;
        }
    } catch (...) {
        ::kill(pid, SIGTERM);
        while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
        throw;
    }
    if (received != 0) {
        while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
        if (received != static_cast<int>(sizeof(childError)))
            throw std::runtime_error("Incomplete exec error message");
        throw std::system_error(childError, std::generic_category(), "child setup/execv");
    }
    process->pid = pid;
    return process.release();
}

int os::WaitProc(ProcessHandle process) {
    if (!process) throw std::invalid_argument("Null process handle");
    if (!process->waited) {
        int status = 0;
        pid_t result;
        do { result = ::waitpid(process->pid, &status, 0); } while (result < 0 && errno == EINTR);
        if (result < 0) Fail("waitpid");
        process->result = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        process->waited = true;
    }
    return process->result;
}
void os::CloseProc(ProcessHandle process) noexcept { delete process; }
void os::KillProc(ProcessHandle process, std::uint32_t) {
    if (!process) throw std::invalid_argument("Null process handle");
    if (::kill(process->pid, SIGTERM) < 0) Fail("kill");
}
std::string os::ExecutablePath() {
    std::vector<char> buffer(512);
    for (;;) {
        const ssize_t n = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (n < 0) Fail("readlink(/proc/self/exe)");
        if (static_cast<std::size_t>(n) < buffer.size()) return std::string(buffer.data(), n);
        buffer.resize(buffer.size() * 2);
    }
}
void os::PreparePipeIO() {
    struct sigaction action{};
    action.sa_handler = SIG_IGN;
    ::sigemptyset(&action.sa_mask);
    if (::sigaction(SIGPIPE, &action, nullptr) < 0) Fail("sigaction(SIGPIPE)");
}

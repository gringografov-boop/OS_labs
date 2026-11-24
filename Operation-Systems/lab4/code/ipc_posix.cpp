#include "ipc.h"

#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <semaphore.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace {
struct Region {
    ipc::Block block;
    sem_t request;
    sem_t response;
};

[[noreturn]] void Fail(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}

bool WaitSemaphore(sem_t* semaphore, const ipc::PeerAlive& alive) {
    for (;;) {
        timespec deadline{};
        if (::clock_gettime(CLOCK_REALTIME, &deadline) < 0) Fail("clock_gettime");
        deadline.tv_nsec += 100 * 1000 * 1000;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000L;
        }
        if (::sem_timedwait(semaphore, &deadline) == 0) return true;
        if (errno == EINTR) continue;
        if (errno != ETIMEDOUT) Fail("sem_timedwait");
        if (alive && !alive()) return ::sem_trywait(semaphore) == 0; // It may have answered last.
    }
}
} // namespace

struct ipc::Channel::Impl {
    Region* region = nullptr;
    std::string path;
    bool owner = false;
    bool semaphoresReady = false;
    ~Impl() {
        if (owner && semaphoresReady) {
            ::sem_destroy(&region->request);
            ::sem_destroy(&region->response);
        }
        if (region) ::munmap(region, sizeof(Region));
        if (owner && !path.empty()) ::unlink(path.c_str());
    }
};

ipc::Channel::Channel(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ipc::Channel::~Channel() = default;

std::unique_ptr<ipc::Channel> ipc::Channel::Create() {
    const char* directory = std::getenv("TMPDIR");
    std::string pattern = std::string(directory && *directory ? directory : "/tmp") + "/lab4_map_XXXXXX";
    const int fd = ::mkstemp(pattern.data());
    if (fd < 0) Fail("mkstemp");
    auto impl = std::make_unique<Impl>();
    impl->path = pattern; // From here the destructor removes the file.
    impl->owner = true;
    if (::ftruncate(fd, static_cast<off_t>(sizeof(Region))) < 0) {
        const int saved = errno;
        ::close(fd);
        errno = saved;
        Fail("ftruncate");
    }
    void* memory = ::mmap(nullptr, sizeof(Region), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    const int saved = errno;
    ::close(fd);
    if (memory == MAP_FAILED) {
        errno = saved;
        Fail("mmap");
    }
    impl->region = static_cast<Region*>(memory);
    std::memset(impl->region, 0, sizeof(Region));
    if (::sem_init(&impl->region->request, 1, 0) < 0) Fail("sem_init(request)");
    if (::sem_init(&impl->region->response, 1, 0) < 0) {
        const int error = errno;
        ::sem_destroy(&impl->region->request);
        errno = error;
        Fail("sem_init(response)");
    }
    impl->semaphoresReady = true;
    return std::unique_ptr<Channel>(new Channel(std::move(impl)));
}

std::unique_ptr<ipc::Channel> ipc::Channel::Open(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) Fail("open(mapping file)");
    struct stat info{};
    if (::fstat(fd, &info) < 0) {
        const int saved = errno;
        ::close(fd);
        errno = saved;
        Fail("fstat(mapping file)");
    }
    if (info.st_size < static_cast<off_t>(sizeof(Region))) {
        ::close(fd);
        throw std::runtime_error("Mapping file is too small");
    }
    void* memory = ::mmap(nullptr, sizeof(Region), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    const int saved = errno;
    ::close(fd);
    if (memory == MAP_FAILED) {
        errno = saved;
        Fail("mmap");
    }
    auto impl = std::make_unique<Impl>();
    impl->region = static_cast<Region*>(memory);
    impl->path = path;
    return std::unique_ptr<Channel>(new Channel(std::move(impl)));
}

ipc::Block& ipc::Channel::block() noexcept { return impl_->region->block; }
const std::string& ipc::Channel::path() const noexcept { return impl_->path; }

void ipc::Channel::SignalRequest() {
    if (::sem_post(&impl_->region->request) < 0) Fail("sem_post(request)");
}
void ipc::Channel::SignalResponse() {
    if (::sem_post(&impl_->region->response) < 0) Fail("sem_post(response)");
}
bool ipc::Channel::WaitRequest(const PeerAlive& alive) { return WaitSemaphore(&impl_->region->request, alive); }
bool ipc::Channel::WaitResponse(const PeerAlive& alive) { return WaitSemaphore(&impl_->region->response, alive); }

std::uint32_t ipc::CurrentProcessId() { return static_cast<std::uint32_t>(::getpid()); }
bool ipc::IsParentAlive(std::uint32_t parentPid) {
    return static_cast<std::uint32_t>(::getppid()) == parentPid;
}

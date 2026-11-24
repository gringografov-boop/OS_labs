#pragma once
// Shared-memory channel between the parent and the child process.
// The memory is a memory-mapped file (mmap on Linux, CreateFileMapping on Windows);
// request/response notifications use process-shared semaphores (Linux) or
// named events (Windows).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

namespace ipc {
inline constexpr std::size_t kLineCapacity = 256;
inline constexpr std::size_t kErrorCapacity = 256;

enum class Command : std::int32_t { None = 0, Line = 1, Stop = 2 };
enum class Reply : std::int32_t { None = 0, Ready, Written, Ignored, Invalid, Stop, Stopped, Error };

// Layout of the mapped region. Both processes are built from the same sources.
struct Block {
    Command command;
    Reply reply;
    std::uint32_t parentPid;
    std::uint32_t lineLength;
    std::uint32_t lineTooLong; // Non-zero: the input line did not fit into `line`.
    char line[kLineCapacity];
    char errorText[kErrorCapacity];
};
static_assert(std::is_standard_layout_v<Block> && std::is_trivially_copyable_v<Block>,
              "Block must be safe to share between processes");

inline void SetError(Block& block, const std::string& text) {
    const std::size_t size = std::min(text.size(), kErrorCapacity - 1);
    std::memcpy(block.errorText, text.data(), size);
    block.errorText[size] = '\0';
}

// Returns false when the peer died before sending a notification.
using PeerAlive = std::function<bool()>;

class Channel {
public:
    // Parent side: creates the backing file, maps it and creates the sync objects.
    static std::unique_ptr<Channel> Create();
    // Child side: opens the backing file created by the parent (UTF-8 path).
    static std::unique_ptr<Channel> Open(const std::string& path);
    ~Channel();
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    Block& block() noexcept;
    const std::string& path() const noexcept; // UTF-8 path of the backing file.

    void SignalRequest();  // parent -> child
    void SignalResponse(); // child -> parent
    // Wait functions poll `alive` every 100 ms and return false if the peer is gone.
    bool WaitRequest(const PeerAlive& alive);
    bool WaitResponse(const PeerAlive& alive);

private:
    struct Impl;
    explicit Channel(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

std::uint32_t CurrentProcessId();
bool IsParentAlive(std::uint32_t parentPid);
} // namespace ipc

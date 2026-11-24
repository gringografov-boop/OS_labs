#include "ipc.h"

#include <windows.h>
#include <climits>
#include <stdexcept>
#include <system_error>

namespace {
[[noreturn]] void Fail(const char* operation) {
    throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), operation);
}

std::wstring Wide(const std::string& text) {
    if (text.empty()) return {};
    if (text.size() > INT_MAX) throw std::length_error("UTF-8 string too large");
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                          static_cast<int>(text.size()), nullptr, 0);
    if (!size) Fail("MultiByteToWideChar");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (!::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                              static_cast<int>(text.size()), result.data(), size))
        Fail("MultiByteToWideChar");
    return result;
}

std::string Utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (!size) Fail("WideCharToMultiByte");
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                              result.data(), size, nullptr, nullptr))
        Fail("WideCharToMultiByte");
    return result;
}

// Event names are derived from the unique backing file name.
std::wstring EventName(const std::wstring& path, const wchar_t* suffix) {
    const auto slash = path.find_last_of(L"\\/");
    const std::wstring base = slash == std::wstring::npos ? path : path.substr(slash + 1);
    return L"Local\\lab4_" + base + suffix;
}

bool WaitEvent(HANDLE event, const ipc::PeerAlive& alive) {
    for (;;) {
        const DWORD result = ::WaitForSingleObject(event, 100);
        if (result == WAIT_OBJECT_0) return true;
        if (result != WAIT_TIMEOUT) Fail("WaitForSingleObject");
        if (alive && !alive()) return ::WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
    }
}
} // namespace

struct ipc::Channel::Impl {
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE mapping = nullptr;
    HANDLE request = nullptr;
    HANDLE response = nullptr;
    ipc::Block* block = nullptr;
    std::string path;
    ~Impl() {
        if (block) ::UnmapViewOfFile(block);
        if (request) ::CloseHandle(request);
        if (response) ::CloseHandle(response);
        if (mapping) ::CloseHandle(mapping);
        if (file != INVALID_HANDLE_VALUE) ::CloseHandle(file); // DELETE_ON_CLOSE removes the file.
    }
    void Map() {
        mapping = ::CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, sizeof(ipc::Block), nullptr);
        if (!mapping) Fail("CreateFileMappingW");
        void* view = ::MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ipc::Block));
        if (!view) Fail("MapViewOfFile");
        block = static_cast<ipc::Block*>(view);
    }
};

ipc::Channel::Channel(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ipc::Channel::~Channel() = default;

std::unique_ptr<ipc::Channel> ipc::Channel::Create() {
    wchar_t directory[MAX_PATH + 1];
    const DWORD length = ::GetTempPathW(MAX_PATH, directory);
    if (!length || length > MAX_PATH) Fail("GetTempPathW");
    wchar_t name[MAX_PATH];
    if (!::GetTempFileNameW(directory, L"lm4", 0, name)) Fail("GetTempFileNameW"); // Creates the file.
    auto impl = std::make_unique<Impl>();
    impl->file = ::CreateFileW(name, GENERIC_READ | GENERIC_WRITE | DELETE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (impl->file == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        ::DeleteFileW(name);
        ::SetLastError(error);
        Fail("CreateFileW(mapping file)");
    }
    impl->path = Utf8(name);
    impl->Map();
    std::memset(impl->block, 0, sizeof(ipc::Block));
    impl->request = ::CreateEventW(nullptr, FALSE, FALSE, EventName(name, L"_req").c_str());
    if (!impl->request) Fail("CreateEventW(request)");
    impl->response = ::CreateEventW(nullptr, FALSE, FALSE, EventName(name, L"_resp").c_str());
    if (!impl->response) Fail("CreateEventW(response)");
    return std::unique_ptr<Channel>(new Channel(std::move(impl)));
}

std::unique_ptr<ipc::Channel> ipc::Channel::Open(const std::string& path) {
    const std::wstring name = Wide(path);
    auto impl = std::make_unique<Impl>();
    impl->file = ::CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (impl->file == INVALID_HANDLE_VALUE) Fail("CreateFileW(mapping file)");
    impl->path = path;
    impl->Map();
    impl->request = ::OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, EventName(name, L"_req").c_str());
    if (!impl->request) Fail("OpenEventW(request)");
    impl->response = ::OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, EventName(name, L"_resp").c_str());
    if (!impl->response) Fail("OpenEventW(response)");
    return std::unique_ptr<Channel>(new Channel(std::move(impl)));
}

ipc::Block& ipc::Channel::block() noexcept { return *impl_->block; }
const std::string& ipc::Channel::path() const noexcept { return impl_->path; }

void ipc::Channel::SignalRequest() {
    if (!::SetEvent(impl_->request)) Fail("SetEvent(request)");
}
void ipc::Channel::SignalResponse() {
    if (!::SetEvent(impl_->response)) Fail("SetEvent(response)");
}
bool ipc::Channel::WaitRequest(const PeerAlive& alive) { return WaitEvent(impl_->request, alive); }
bool ipc::Channel::WaitResponse(const PeerAlive& alive) { return WaitEvent(impl_->response, alive); }

std::uint32_t ipc::CurrentProcessId() { return static_cast<std::uint32_t>(::GetCurrentProcessId()); }
bool ipc::IsParentAlive(std::uint32_t parentPid) {
    HANDLE process = ::OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (!process) return false;
    const bool alive = ::WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    ::CloseHandle(process);
    return alive;
}

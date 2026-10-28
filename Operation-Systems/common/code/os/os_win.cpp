#include "os.h"

#include <windows.h>
#include <climits>
#include <memory>
#include <stdexcept>
#include <system_error>

struct os::FileHandleDummy {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~FileHandleDummy() {
        if (handle && handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle);
    }
};
struct os::ProcessHandleDummy {
    HANDLE handle = nullptr;
    ~ProcessHandleDummy() { if (handle) ::CloseHandle(handle); }
};
namespace {
[[noreturn]] void Fail(const char* operation) {
    const DWORD error = ::GetLastError();
    throw std::system_error(static_cast<int>(error), std::system_category(), operation);
}
HANDLE Native(os::FileHandle file) {
    if (!file) throw std::invalid_argument("Null file handle");
    return file->handle;
}
std::wstring Wide(const std::string& text) {
    if (text.empty()) return {};
    if (text.size() > INT_MAX) throw std::length_error("UTF-8 string too large");
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                          static_cast<int>(text.size()), nullptr, 0);
    if (!size) Fail("MultiByteToWideChar");
    std::wstring result(size, L'\0');
    if (!::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                              static_cast<int>(text.size()), result.data(), size))
        Fail("MultiByteToWideChar");
    return result;
}
std::string Utf8(const wchar_t* text, int size) {
    const int count = ::WideCharToMultiByte(CP_UTF8, 0, text, size, nullptr, 0, nullptr, nullptr);
    if (!count) Fail("WideCharToMultiByte");
    std::string result(count, '\0');
    if (!::WideCharToMultiByte(CP_UTF8, 0, text, size, result.data(), count, nullptr, nullptr))
        Fail("WideCharToMultiByte");
    return result;
}
// Supply valid inheritable stdio handles even when the parent's streams are redirected.
void DuplicateStandard(os::FileHandle redirect, DWORD standard, os::FileHandleDummy& output) {
    const HANDLE source = redirect ? Native(redirect) : ::GetStdHandle(standard);
    if (!source || source == INVALID_HANDLE_VALUE) {
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        output.handle = ::CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output.handle == INVALID_HANDLE_VALUE) Fail("CreateFileW(NUL)");
    } else if (!::DuplicateHandle(::GetCurrentProcess(), source, ::GetCurrentProcess(),
                                  &output.handle, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        Fail("DuplicateHandle(stdio)");
    }
}
struct AttributeList {
    LPPROC_THREAD_ATTRIBUTE_LIST value = nullptr;
    ~AttributeList() { if (value) ::DeleteProcThreadAttributeList(value); }
};
}

bool os::CreatePipe(FileHandle& readPipe, FileHandle& writePipe) {
    auto read = std::make_unique<FileHandleDummy>();
    auto write = std::make_unique<FileHandleDummy>();
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!::CreatePipe(&read->handle, &write->handle, &attributes, 0)) Fail("CreatePipe");
    readPipe = read.release();
    writePipe = write.release();
    return true;
}
void os::SetInheritable(FileHandle file, bool inherit) {
    if (!::SetHandleInformation(Native(file), HANDLE_FLAG_INHERIT, inherit ? HANDLE_FLAG_INHERIT : 0))
        Fail("SetHandleInformation");
}
os::FileHandle os::OpenWrite(const std::string& filename) {
    auto file = std::make_unique<FileHandleDummy>();
    const std::wstring name = Wide(filename);
    file->handle = ::CreateFileW(name.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file->handle == INVALID_HANDLE_VALUE) Fail("CreateFileW(output file)");
    return file.release();
}
void os::CloseFile(FileHandle file) noexcept { delete file; }
int os::PipeRead(FileHandle file, void* buffer, int count) {
    if (count < 0) throw std::invalid_argument("Negative read size");
    DWORD received = 0;
    if (!::ReadFile(Native(file), buffer, static_cast<DWORD>(count), &received, nullptr)) {
        if (::GetLastError() == ERROR_BROKEN_PIPE) return 0;
        Fail("ReadFile");
    }
    return static_cast<int>(received);
}
int os::PipeWrite(FileHandle file, const void* buffer, int count) {
    if (count < 0) throw std::invalid_argument("Negative write size");
    DWORD written = 0;
    if (!::WriteFile(Native(file), buffer, static_cast<DWORD>(count), &written, nullptr))
        Fail("WriteFile");
    return static_cast<int>(written);
}
os::ProcessHandle os::CreateProc(const ProcessStartInfo& info) {
    if (info.executeCmdAsShell) throw std::invalid_argument("Shell execution is disabled");
    auto process = std::make_unique<ProcessHandleDummy>();
    const std::wstring app = Wide(info.cmd);
    std::wstring command = Wide(GenerateAppCmd(info.cmd.c_str(), info.args));
    const std::wstring directory = Wide(info.workDir);
    FileHandleDummy input, output, error;
    DuplicateStandard(info.redirectStdin, STD_INPUT_HANDLE, input);
    DuplicateStandard(info.redirectStdout, STD_OUTPUT_HANDLE, output);
    DuplicateStandard(info.redirectStderr, STD_ERROR_HANDLE, error);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.handle;
    startup.StartupInfo.hStdOutput = output.handle;
    startup.StartupInfo.hStdError = error.handle;
    HANDLE inherited[] = {input.handle, output.handle, error.handle};
    SIZE_T size = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<unsigned char> buffer(size);
    AttributeList attributes;
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
    if (!::InitializeProcThreadAttributeList(list, 1, 0, &size))
        Fail("InitializeProcThreadAttributeList");
    attributes.value = list;
    if (!::UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                     inherited, sizeof(inherited), nullptr, nullptr))
        Fail("UpdateProcThreadAttribute");
    startup.lpAttributeList = list;

    PROCESS_INFORMATION result{};
    const DWORD flags = EXTENDED_STARTUPINFO_PRESENT |
                         ((info.flags & ProcessStartInfo::HIDDEN) ? CREATE_NO_WINDOW : 0);
    if (!::CreateProcessW(app.c_str(), command.data(), nullptr, nullptr, TRUE, flags,
                          nullptr, directory.empty() ? nullptr : directory.c_str(),
                          &startup.StartupInfo, &result))
        Fail("CreateProcessW");
    process->handle = result.hProcess;
    ::CloseHandle(result.hThread); // Closing the handle does not stop the thread.
    return process.release();
}
int os::WaitProc(ProcessHandle process) {
    if (!process) throw std::invalid_argument("Null process handle");
    if (::WaitForSingleObject(process->handle, INFINITE) != WAIT_OBJECT_0)
        Fail("WaitForSingleObject");
    DWORD code = 0;
    if (!::GetExitCodeProcess(process->handle, &code)) Fail("GetExitCodeProcess");
    return code <= INT_MAX ? static_cast<int>(code) : 1;
}
void os::CloseProc(ProcessHandle process) noexcept { delete process; }
void os::KillProc(ProcessHandle process, std::uint32_t code) {
    if (!process) throw std::invalid_argument("Null process handle");
    if (!::TerminateProcess(process->handle, code)) Fail("TerminateProcess");
}
std::string os::ExecutablePath() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD size = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!size) Fail("GetModuleFileNameW");
        if (size < buffer.size()) return Utf8(buffer.data(), static_cast<int>(size));
        buffer.resize(buffer.size() * 2);
    }
}
void os::PreparePipeIO() {}

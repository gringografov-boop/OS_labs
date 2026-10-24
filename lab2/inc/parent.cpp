#include "protocol.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
    void print_status(char status)
    {
        if (status == protocol::written)
            std::cout << "Composite number was written to the file.\n";
        else if (status == protocol::ignored)
            std::cout << "0 and 1 are neither prime nor composite; value was ignored.\n";
        else if (status == protocol::invalid)
            std::cout << "Child rejected the command; enter one integer.\n";
        else if (status == protocol::stop)
            std::cout << "Negative or prime number received. Processes are stopping.\n";
        else
            std::cerr << "Unknown child status.\n";
    }

#ifdef _WIN32
    class unique_handle
    {
    public:
        explicit unique_handle(HANDLE handle = nullptr) noexcept : handle_(handle) {}
        unique_handle(const unique_handle &) = delete;
        unique_handle &operator=(const unique_handle &) = delete;
        ~unique_handle() { reset(); }
        HANDLE get() const noexcept { return handle_; }
        void reset(HANDLE handle = nullptr) noexcept
        {
            if (handle_ && handle_ != INVALID_HANDLE_VALUE)
                CloseHandle(handle_);
            handle_ = handle;
        }

    private:
        HANDLE handle_;
    };

    int windows_error(const char *operation)
    {
        std::cerr << operation << " failed, Windows error: " << GetLastError() << '\n';
        return 1;
    }

    bool child_path(std::string &path)
    {
        std::array<char, 4096> buffer{};
        const DWORD length = GetModuleFileNameA(nullptr, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
            return false;
        path.assign(buffer.data(), length);
        const auto slash = path.find_last_of("\\/");
        if (slash == std::string::npos)
            return false;
        path.resize(slash + 1);
        path += "child.exe";
        return true;
    }
#else
    class unique_fd
    {
    public:
        explicit unique_fd(int fd = -1) noexcept : fd_(fd) {}
        unique_fd(const unique_fd &) = delete;
        unique_fd &operator=(const unique_fd &) = delete;
        ~unique_fd() { reset(); }
        int get() const noexcept { return fd_; }
        void reset(int fd = -1) noexcept
        {
            if (fd_ >= 0)
                close(fd_);
            fd_ = fd;
        }

    private:
        int fd_;
    };

    bool child_path(std::string &path)
    {
        std::array<char, PATH_MAX> buffer{};
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (length < 0)
        {
            std::perror("readlink");
            return false;
        }
        buffer[static_cast<std::size_t>(length)] = '\0';
        path.assign(buffer.data());
        const auto slash = path.find_last_of('/');
        if (slash == std::string::npos)
            return false;
        path.resize(slash + 1);
        path += "child";
        return true;
    }
#endif
}

#ifdef _WIN32
int main()
{
    std::string filename;
    std::cout << "Output file: ";
    if (!std::getline(std::cin, filename) || filename.empty())
        return 1;
    if (filename.find('"') != std::string::npos)
        return 1;

    std::string executable;
    if (!child_path(executable))
        return 1;

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE input_read_raw = nullptr, input_write_raw = nullptr,
           status_read_raw = nullptr, status_write_raw = nullptr;

    if (!CreatePipe(&input_read_raw, &input_write_raw, &attributes, 0))
        return windows_error("CreatePipe(input)");
    unique_handle input_read(input_read_raw), input_write(input_write_raw);
    if (!SetHandleInformation(input_write.get(), HANDLE_FLAG_INHERIT, 0))
        return windows_error("SetHandleInformation(input)");

    if (!CreatePipe(&status_read_raw, &status_write_raw, &attributes, 0))
        return windows_error("CreatePipe(status)");
    unique_handle status_read(status_read_raw), status_write(status_write_raw);
    if (!SetHandleInformation(status_read.get(), HANDLE_FLAG_INHERIT, 0))
        return windows_error("SetHandleInformation(status)");

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input_read.get();
    startup.hStdOutput = status_write.get();
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    std::string command = "\"" + executable + "\" \"" + filename + "\"";

    if (!CreateProcessA(
            executable.c_str(), // 1. Путь к запускаемому .exe
            command.data(),     // 2. Командная строка с аргументами
            nullptr,            // 3. Настройки безопасности дескриптора процесса
            nullptr,            // 4. Настройки безопасности дескриптора потока
            TRUE,               // 5. Разрешить наследование дескрипторов
            0,                  // 6. Нет дополнительных флагов запуска
            nullptr,            // 7. Использовать окружение родителя
            nullptr,            // 8. Использовать рабочую папку родителя
            &startup,           // 9. Настройки запуска
            &process            // 10. Куда записать сведения о процессе
            ))
        return windows_error("CreateProcessA");
    unique_handle process_handle(process.hProcess), thread_handle(process.hThread);
    input_read.reset();
    status_write.reset();

    std::string line;
    for (;;)
    {
        std::cout << "Number: ";
        if (!std::getline(std::cin, line))
            break;
        line.push_back('\n');
        DWORD written = 0;
        if (!WriteFile(input_write.get(), line.data(), static_cast<DWORD>(line.size()),
                       &written, nullptr) ||
            written != line.size())
            break;
        char status = 0;
        DWORD received = 0;
        if (!ReadFile(status_read.get(), &status, 1, &received, nullptr) || received != 1)
            break;
        print_status(status);
        if (status == protocol::stop)
            break;
    }
    input_write.reset();
    status_read.reset();
    WaitForSingleObject(process_handle.get(), INFINITE);
    return 0;
}
#else
int main()
{
    std::string filename;
    std::cout << "Output file: ";
    if (!std::getline(std::cin, filename) || filename.empty())
        return 1;

    std::string executable;
    if (!child_path(executable))
        return 1;

    int parent_to_child[2]{};
    int child_to_parent[2]{};
    if (pipe(parent_to_child) < 0)
    {
        std::perror("pipe(parent_to_child)");
        return 1;
    }
    unique_fd input_read(parent_to_child[0]), input_write(parent_to_child[1]);
    if (pipe(child_to_parent) < 0)
    {
        std::perror("pipe(child_to_parent)");
        return 1;
    }
    unique_fd status_read(child_to_parent[0]), status_write(child_to_parent[1]);

    const pid_t pid = fork();
    if (pid < 0)
    {
        std::perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        if (dup2(input_read.get(), STDIN_FILENO) < 0 ||
            dup2(status_write.get(), STDOUT_FILENO) < 0)
        {
            std::perror("dup2");
            _exit(1);
        }
        input_read.reset();
        input_write.reset();
        status_read.reset();
        status_write.reset();
        execl(executable.c_str(), "child", filename.c_str(), static_cast<char *>(nullptr));
        std::perror("execl");
        _exit(1);
    }

    input_read.reset();
    status_write.reset();
    std::string line;
    for (;;)
    {
        std::cout << "Number: ";
        if (!std::getline(std::cin, line))
            break;
        line.push_back('\n');
        const char *data = line.data();
        std::size_t remaining = line.size();
        while (remaining > 0)
        {
            const ssize_t count = write(input_write.get(), data, remaining);
            if (count < 0)
            {
                std::perror("write");
                return 1;
            }
            data += count;
            remaining -= static_cast<std::size_t>(count);
        }
        char status = 0;
        if (read(status_read.get(), &status, 1) != 1)
            break;
        print_status(status);
        if (status == protocol::stop)
            break;
    }
    input_write.reset();
    status_read.reset();
    int status = 0;
    waitpid(pid, &status, 0);
    return 0;
}
#endif

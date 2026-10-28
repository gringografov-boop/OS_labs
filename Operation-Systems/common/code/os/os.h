#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace os {
struct FileHandleDummy;
using FileHandle = FileHandleDummy*;
struct ProcessHandleDummy;
using ProcessHandle = ProcessHandleDummy*;

struct ProcessStartInfo {
    enum FLAGS : unsigned char { FOCUSED = 0, HIDDEN = 1, USE_PARENT_CONSOLE = 2, LAST = 3 };
    std::string cmd;
    std::vector<std::string> args; // Arguments without argv[0].
    std::string workDir;
    unsigned char flags = USE_PARENT_CONSOLE;
    bool executeCmdAsShell = false; // Shell commands are intentionally not supported.
    FileHandle redirectStdin = nullptr;
    FileHandle redirectStdout = nullptr;
    FileHandle redirectStderr = nullptr;
};

// Operations throw std::system_error on OS errors, unless marked noexcept.
// Returned handles have one owner; close them exactly once.
bool CreatePipe(FileHandle& readPipe, FileHandle& writePipe);
void SetInheritable(FileHandle file, bool inherit);
FileHandle OpenWrite(const std::string& filename); // Create or truncate the file.
void CloseFile(FileHandle file) noexcept;
int PipeRead(FileHandle file, void* buffer, int count); // 0 means EOF.
int PipeWrite(FileHandle file, const void* buffer, int count);
void WriteAll(FileHandle file, const void* buffer, int count);

// Corrected upstream return type: CreateProc returns a process handle, not bool.
ProcessHandle CreateProc(const ProcessStartInfo& info);
int WaitProc(ProcessHandle process); // Exit code, or 128 + signal on Linux.
void CloseProc(ProcessHandle process) noexcept; // Does not terminate the process.
void KillProc(ProcessHandle process, std::uint32_t code = 1);
std::string ExecutablePath();
std::string GenerateAppCmd(const char* cmd, const std::vector<std::string>& args);
void PreparePipeIO(); // Linux: turn SIGPIPE into a write error; Windows: no-op.
} // namespace os

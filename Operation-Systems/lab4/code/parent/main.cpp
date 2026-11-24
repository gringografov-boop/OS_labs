#include "ipc.h"
#include "os/os.h"

#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
std::string ChildPath() {
    std::string path = os::ExecutablePath();
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string::npos) throw std::runtime_error("Cannot determine executable directory");
    path.resize(slash + 1);
#ifdef _WIN32
    return path + "lab4_child.exe";
#else
    return path + "lab4_child";
#endif
}

// Returns true when the child asked to stop.
bool HandleReply(const ipc::Block& block) {
    switch (block.reply) {
    case ipc::Reply::Written: std::cout << "Composite number was written to the file.\n"; return false;
    case ipc::Reply::Ignored: std::cout << "0 and 1 are neither prime nor composite; value was ignored.\n"; return false;
    case ipc::Reply::Invalid: std::cout << "Invalid input: enter one integer in the int range.\n"; return false;
    case ipc::Reply::Stop: std::cout << "Negative or prime number received. Processes are stopping.\n"; return true;
    case ipc::Reply::Error: throw std::runtime_error(std::string("Child error: ") + block.errorText);
    default: throw std::runtime_error("Unknown child reply");
    }
}
} // namespace

int main() {
    std::string filename;
    std::cout << "Output file: ";
    if (!std::getline(std::cin, filename) || filename.empty() || filename.find('\0') != std::string::npos) {
        std::cerr << "Invalid output file name.\n";
        return 1;
    }
    std::unique_ptr<ipc::Channel> channel; // Outlives the child process.
    os::ProcessHandle child = nullptr;
    int result = 0;
    try {
        channel = ipc::Channel::Create();
        ipc::Block& block = channel->block();
        block.parentPid = ipc::CurrentProcessId();

        os::ProcessStartInfo info;
        info.cmd = ChildPath();
        info.args = {channel->path(), filename};
        child = os::CreateProc(info);
        const ipc::PeerAlive alive = [&child] { return os::IsProcRunning(child); };

        // Do not accept commands until the child has opened the output file.
        if (!channel->WaitResponse(alive)) throw std::runtime_error("Child terminated before it was ready");
        if (block.reply == ipc::Reply::Error) throw std::runtime_error(std::string("Child error: ") + block.errorText);
        if (block.reply != ipc::Reply::Ready) throw std::runtime_error("Invalid child startup status");

        const auto exchange = [&](ipc::Command command) {
            block.reply = ipc::Reply::None;
            block.command = command;
            channel->SignalRequest();
            if (!channel->WaitResponse(alive)) throw std::runtime_error("Child terminated unexpectedly");
        };
        std::string line;
        bool stopped = false;
        for (;;) {
            std::cout << "Number: ";
            if (!std::getline(std::cin, line)) {
                if (!std::cin.eof()) throw std::runtime_error("Input stream error");
                break;
            }
            block.lineTooLong = line.size() >= ipc::kLineCapacity ? 1 : 0;
            block.lineLength = block.lineTooLong ? 0 : static_cast<std::uint32_t>(line.size());
            if (!block.lineTooLong) std::memcpy(block.line, line.data(), line.size());
            exchange(ipc::Command::Line);
            if (HandleReply(block)) { stopped = true; break; }
        }
        if (!stopped) {
            exchange(ipc::Command::Stop); // End of input: ask the child to finish.
            if (block.reply == ipc::Reply::Error) HandleReply(block);
        }
    } catch (const std::exception& error) {
        std::cerr << "parent: " << error.what() << '\n';
        result = 1;
        if (child) {
            try { if (os::IsProcRunning(child)) os::KillProc(child); } catch (...) {}
        }
    }
    if (child) {
        try {
            if (os::WaitProc(child) != 0) result = 1;
        } catch (const std::exception& error) {
            std::cerr << "parent wait: " << error.what() << '\n';
            result = 1;
        }
        os::CloseProc(child);
    }
    channel.reset();
    return result;
}

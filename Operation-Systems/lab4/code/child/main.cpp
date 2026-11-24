#include "ipc.h"
#include "os/os.h"
#include "resources.h"
#include "common.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
int Run(const std::string& mappingPath, const std::string& filename) {
    auto channel = ipc::Channel::Open(mappingPath);
    ipc::Block& block = channel->block();
    const std::uint32_t parentPid = block.parentPid;
    const ipc::PeerAlive alive = [parentPid] { return ipc::IsParentAlive(parentPid); };
    const auto respond = [&](ipc::Reply reply) {
        block.reply = reply;
        channel->SignalResponse();
    };

    UniqueFile output(os::OpenWrite(filename));
    respond(ipc::Reply::Ready);
    for (;;) {
        if (!channel->WaitRequest(alive)) throw std::runtime_error("Parent terminated unexpectedly");
        if (block.command == ipc::Command::Stop) {
            respond(ipc::Reply::Stopped);
            return 0;
        }
        if (block.lineTooLong || block.lineLength >= ipc::kLineCapacity) {
            respond(ipc::Reply::Invalid);
            continue;
        }
        const auto value = ParseInteger(std::string(block.line, block.lineLength));
        if (!value) {
            respond(ipc::Reply::Invalid);
        } else if (*value < 0 || IsPrime(*value)) {
            respond(ipc::Reply::Stop);
            return 0;
        } else if (*value < 2) {
            respond(ipc::Reply::Ignored);
        } else {
            const std::string text = std::to_string(*value) + '\n';
            try {
                os::WriteAll(output.get(), text.data(), static_cast<int>(text.size()));
            } catch (const std::exception& error) {
                ipc::SetError(block, error.what());
                respond(ipc::Reply::Error);
                return 1;
            }
            respond(ipc::Reply::Written);
        }
    }
}

#ifdef _WIN32
std::string ToUtf8(const wchar_t* text) {
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Cannot convert argument to UTF-8");
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!::WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr))
        throw std::runtime_error("Cannot convert argument to UTF-8");
    result.pop_back();
    return result;
}
#endif
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    if (argc != 3) {
        std::cerr << "Usage: lab4_child <mapping-file> <output-file>\n";
        return 1;
    }
    try {
#ifdef _WIN32
        return Run(ToUtf8(argv[1]), ToUtf8(argv[2]));
#else
        return Run(argv[1], argv[2]);
#endif
    } catch (const std::exception& error) {
        std::cerr << "child: " << error.what() << '\n';
        return 1;
    }
}

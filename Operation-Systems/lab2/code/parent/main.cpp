#include "os/os.h"
#include "protocol.h"
#include "resources.h"

#include <climits>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::string ChildPath() {
    std::string path = os::ExecutablePath();
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string::npos) throw std::runtime_error("Cannot determine executable directory");
    path.resize(slash + 1);
#ifdef _WIN32
    return path + "lab2_child.exe";
#else
    return path + "lab2_child";
#endif
}
char ReadStatus(os::FileHandle pipe) {
    char status = 0;
    if (os::PipeRead(pipe, &status, 1) != 1)
        throw std::runtime_error("Child closed the status pipe unexpectedly");
    return status;
}
void PrintStatus(char status) {
    switch (status) {
    case protocol::written: std::cout << "Composite number was written to the file.\n"; break;
    case protocol::ignored: std::cout << "0 and 1 are neither prime nor composite; value was ignored.\n"; break;
    case protocol::invalid: std::cout << "Invalid input: enter one integer in the int range.\n"; break;
    case protocol::stop: std::cout << "Negative or prime number received. Processes are stopping.\n"; break;
    default: throw std::runtime_error("Unknown child status");
    }
}
}

int main() {
    std::string filename;
    std::cout << "Output file: ";
    if (!std::getline(std::cin, filename) || filename.empty() || filename.find('\0') != std::string::npos) {
        std::cerr << "Invalid output file name.\n";
        return 1;
    }
    os::ProcessHandle child = nullptr;
    int result = 0;
    try {
        os::PreparePipeIO();
        os::FileHandle read = nullptr, write = nullptr;
        os::CreatePipe(read, write);
        UniqueFile inputRead(read), inputWrite(write);
        os::SetInheritable(inputWrite.get(), false);
        os::CreatePipe(read, write);
        UniqueFile statusRead(read), statusWrite(write);
        os::SetInheritable(statusRead.get(), false);

        os::ProcessStartInfo info;
        info.cmd = ChildPath();
        info.args = {filename};
        info.redirectStdin = inputRead.get();
        info.redirectStdout = statusWrite.get();
        child = os::CreateProc(info);
        inputRead.reset();
        statusWrite.reset();

        // Do not accept commands until the child has opened the output file.
        if (ReadStatus(statusRead.get()) != protocol::ready)
            throw std::runtime_error("Invalid child startup status");
        std::string line;
        for (;;) {
            std::cout << "Number: ";
            if (!std::getline(std::cin, line)) {
                if (!std::cin.eof()) throw std::runtime_error("Input stream error");
                break;
            }
            if (line.size() >= INT_MAX) throw std::length_error("Input line is too long");
            line += '\n';
            os::WriteAll(inputWrite.get(), line.data(), static_cast<int>(line.size()));
            const char status = ReadStatus(statusRead.get());
            PrintStatus(status);
            if (status == protocol::stop) break;
        }
        // Scope exit closes all pipes before waiting; child can observe EOF.
    } catch (const std::exception& error) {
        std::cerr << "parent: " << error.what() << '\n';
        result = 1;
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
    return result;
}

#include "os/os.h"
#include "protocol.h"
#include "resources.h"
#include "common.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <cwchar>
#endif

namespace {
void SendStatus(char status) {
    std::cout.put(status).flush();
    if (!std::cout) throw std::runtime_error("Cannot send status to parent");
}
int Run(const std::string& filename) {
    UniqueFile output(os::OpenWrite(filename));
    SendStatus(protocol::ready);
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto value = ParseInteger(line);
        if (!value) {
            SendStatus(protocol::invalid);
        } else if (*value < 0 || IsPrime(*value)) {
            SendStatus(protocol::stop);
            return 0;
        } else if (*value < 2) {
            SendStatus(protocol::ignored);
        } else {
            const std::string text = std::to_string(*value) + '\n';
            os::WriteAll(output.get(), text.data(), static_cast<int>(text.size()));
            SendStatus(protocol::written);
        }
    }
    if (!std::cin.eof()) throw std::runtime_error("Input stream error");
    return 0;
}
}

#ifdef _WIN32
// wmain receives Unicode file names; convert once for the common UTF-8 API.
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    if (argc != 2) {
        std::cerr << "Usage: child <output-file>\n";
        return 1;
    }
    try {
        os::PreparePipeIO();
#ifdef _WIN32
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, nullptr, 0, nullptr, nullptr);
        if (!size) throw std::runtime_error("Cannot convert file name to UTF-8");
        std::string filename(size, '\0');
        if (!::WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, filename.data(), size, nullptr, nullptr))
            throw std::runtime_error("Cannot convert file name to UTF-8");
        filename.pop_back();
        return Run(filename);
#else
        return Run(argv[1]);
#endif
    } catch (const std::exception& error) {
        std::cerr << "child: " << error.what() << '\n';
        return 1;
    }
}

#include "os.h"

#include <stdexcept>

namespace {
// Windows argv quoting: double backslashes before a quote or closing quote.
std::string QuoteArgument(const std::string& argument) {
    std::string result = "\"";
    std::size_t slashes = 0;
    for (char c : argument) {
        if (c == '\\') {
            ++slashes;
        } else {
            result.append(c == '"' ? 2 * slashes + 1 : slashes, '\\');
            result += c;
            slashes = 0;
        }
    }
    result.append(2 * slashes, '\\');
    result += '"';
    return result;
}
}

std::string os::GenerateAppCmd(const char* cmd, const std::vector<std::string>& args) {
    std::string result = QuoteArgument(cmd);
    for (const auto& argument : args) result += " " + QuoteArgument(argument);
    return result;
}

void os::WriteAll(FileHandle file, const void* buffer, int count) {
    if (count < 0) throw std::invalid_argument("Negative write size");
    const char* data = static_cast<const char*>(buffer);
    while (count > 0) {
        const int written = PipeWrite(file, data, count);
        if (written <= 0) throw std::runtime_error("Write made no progress");
        data += written;
        count -= written;
    }
}

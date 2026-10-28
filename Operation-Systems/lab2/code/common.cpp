#include "common.h"
#include <charconv>
#include <cctype>
#include <system_error>

std::optional<int> ParseInteger(const std::string& text) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
    while (begin < end && std::isspace(static_cast<unsigned char>(end[-1]))) --end;
    if (begin == end) return std::nullopt;
    if (*begin == '+') {
        ++begin;
        if (begin == end || *begin < '0' || *begin > '9') return std::nullopt;
    }
    int value = 0;
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
    return value;
}

bool IsPrime(int value) {
    if (value < 2) return false;
    if (value == 2) return true;
    if (value % 2 == 0) return false;
    for (int divisor = 3; divisor <= value / divisor; divisor += 2)
        if (value % divisor == 0) return false;
    return true;
}

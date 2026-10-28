#pragma once
#include "defines.h"

#define ASSERT_MSG(cond, fmt, ...)                                                             \
    if (!(cond)) {                                                                             \
        std::string err_str = err::CreateReport(__FILE__, __LINE__, #cond, fmt, #__VA_ARGS__); \
        std::cerr << err_str << std::endl;                                                     \
        err::DebugBreak();                                                                     \
    }

namespace err {
template <typename... Args>
std::string CreateReport(const char* file, int line, const char* condition, const char* fmt, Args... args);
void DebugBreak();
} // namespace

#include "errors.hpp"
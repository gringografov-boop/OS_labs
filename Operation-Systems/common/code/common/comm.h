#pragma once

/* File with useful includes */

#include "defines.h"
#include "errors.h"

#include <ctime>

using BYTE = unsigned char;

#define Log(stream, level, category, message)                                                           \
    {                                                                                                   \
        auto t = std::time(nullptr);                                                                    \
        auto tm = *std::localtime(&t);                                                                  \
        stream << std::put_time(&tm, "%H-%M-%S") << level << " " << category << " " << message << "\n"; \
    }

#define LogMsg(category, message) \
    Log(std::cout, "MSG", category, message);

#define LogErr(category, message)             \
    Log(std::cerr, "ERR", category, message); \
    Log(std::cout, "ERR", category, message);

#define LogWarn(category, message) \
    Log(std::cout, "WARN", category, message);

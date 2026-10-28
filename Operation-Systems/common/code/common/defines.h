#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>


#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#define OS_WIN
#define _USE_MATH_DEFINES // for C++
#define NOMINMAX
#include <Windows.h>
#include <corecrt_math_defines.h>
#elif defined(__unix__)
#define OS_LINUX
#endif

#define all(v) v.begin(), v.end()
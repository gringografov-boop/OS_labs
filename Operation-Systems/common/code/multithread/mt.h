#pragma once

#include "common/comm.h"
#include <functional>

namespace mt {
struct ThreadHandleDummy { };
typedef ThreadHandleDummy* ThreadHandle;

using FuncNoArgs = std::function<void()>;
typedef void (*ThreadFunc)(void*);

using ThreadID = uint64_t;
using ThreadNameType = std::string;

// Create/Terminate functions
ThreadHandle CreateThread(ThreadFunc func, void* param = nullptr, const ThreadNameType& name = "", int stackSize = -1);
ThreadHandle CreateThread(FuncNoArgs func, const ThreadNameType& name = "", int stackSize = -1);
int ThreadTerminate(ThreadHandle handle);
void DestroyThreadHandle(ThreadHandle handle);

// Join/Detach
void ThreadJoin(ThreadHandle handle, uint32_t msTimeout = -1);
void ThreadDetach(ThreadHandle handle);
};
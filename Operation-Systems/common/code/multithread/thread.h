#pragma once

#include "common/comm.h"
#include "mt.h"

namespace mt {
class Thread {
public:
    static constexpr int defaultStackSize = 128 * 1024;

    Thread(std::string name, ThreadFunc threadFunc, int stackSize = defaultStackSize);
    ~Thread();

    void Run(void* context);
    void Stop();
    void Join(uint32_t msTimeOut = -1);

private:
    std::string name;
    ThreadFunc func;
    int stackSize;
    ThreadHandle handle = nullptr;
};
};
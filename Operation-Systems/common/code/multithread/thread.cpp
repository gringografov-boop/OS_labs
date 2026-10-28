#include "thread.h"

using namespace mt;

Thread::Thread(std::string name, ThreadFunc threadFunc, int stackSize)
    : name(std::move(name))
    , func(threadFunc)
    , stackSize(stackSize)
{
}

Thread::~Thread()
{
    Stop();
}

void Thread::Run(void* context)
{
    Stop();
    handle = CreateThread(func, context, name, stackSize);
}

void Thread::Stop()
{
    if (handle) {
        int ret = ThreadTerminate(handle);
        if (ret) {
            std::string err;
            err.resize(64);
            sprintf(err.data(), "Failed to terminate thread `%s`: 0x%x", name.c_str(), ret);
            LogErr("os/thread", err);
        }
        handle = nullptr;
    }
}

void Thread::Join(uint32_t msTimeOut)
{
    ThreadJoin(handle, msTimeOut);
}

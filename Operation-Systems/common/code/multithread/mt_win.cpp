#include "mt.h"
#include "os/os_win.h"

using namespace mt;

struct CreateThread_DummyProc_Args {
    ThreadFunc func;
    void* param;
};

DWORD WINAPI CreateThread_DummyProc(void* args_)
{
    CreateThread_DummyProc_Args* argsPtr = reinterpret_cast<CreateThread_DummyProc_Args*>(args_);
    CreateThread_DummyProc_Args args = *argsPtr;
    delete argsPtr;

    args.func(args.param);
}

ThreadHandle mt::CreateThread(ThreadFunc func, void* param, const ThreadNameType& name, int stackSize)
{

    CreateThread_DummyProc_Args* args = new CreateThread_DummyProc_Args;
    args->func = func;
    args->param = param;

    if (stackSize == -1) {
        stackSize = 0; // default
    }

    DWORD threadID;
    HANDLE hThread = CreateThread(nullptr, stackSize, CreateThread_DummyProc, args, 0, &threadID);

    if (hThread != nullptr) {
        ThreadNameType buffer;
        if (name != "") {
            buffer = name;
        } else {
            buffer = sprintf(buffer.data(), "Thread<%lu>", threadID);
        }

        SetThreadName(threadID, buffer);
    }

    return reinterpret_cast<ThreadHandle>(hThread);
}

static void CreateThreadFuncNoArgs_Proc(FuncNoArgs* func_)
{
    std::unique_ptr<FuncNoArgs> func(func_);

    (*func)();
}

ThreadHandle mt::CreateThread(FuncNoArgs func, const ThreadNameType& name, int stackSize)
{
    FuncNoArgs* funcArg = new FuncNoArgs(std::move(func));
    return CreateThread((ThreadFunc)&CreateThreadFuncNoArgs_Proc, (void*)funcArg, name, stackSize);
}

int mt::ThreadTerminate(ThreadHandle handle)
{
    BOOL ret = TerminateThread(handle, -1);
    DestroyThreadHandle(handle);
    if (!ret) {
        return static_cast<int>(GetLastError());
    }
    return 0;
}

void mt::DestroyThreadHandle(ThreadHandle handle)
{
    CloseHandle(handle);
}

void mt::ThreadJoin(ThreadHandle handle, uint32_t msTimeout)
{
    WaitForSingleObject(handle, msTimeout);
}

void mt::ThreadDetach(ThreadHandle handle)
{
    CloseHandle(handle);
}

void mt::SetThreadName(ThreadID id, const ThreadNameType& name)
{
}

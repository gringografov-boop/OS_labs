#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdint>
#include "mt.h"
#include <cerrno>
#include <cstring>
#include <memory>
#include <pthread.h>
#include <stdexcept>
#include <time.h>
#include <utility>

namespace {
struct NativeThread {
    pthread_t thread{};
    bool joined = false;
};
}

namespace {
[[noreturn]] void fail(const char* operation, int error) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(error));
}

struct StartArgs {
    mt::ThreadFunc func;
    void* parameter;
    std::string name;
};

void* start_thread(void* opaque) {
    std::unique_ptr<StartArgs> args(static_cast<StartArgs*>(opaque));
    // Linux thread names are optional diagnostics, limited to 15 visible bytes.
    if (!args->name.empty())
        (void)pthread_setname_np(pthread_self(), args->name.c_str());
    const auto function = args->func;
    void* parameter = args->parameter;
    args.reset();
    function(parameter);
    return nullptr;
}

void start_function(void* opaque) {
    std::unique_ptr<mt::FuncNoArgs> function(static_cast<mt::FuncNoArgs*>(opaque));
    (*function)();
}
}

mt::ThreadHandle mt::CreateThread(ThreadFunc function, void* parameter,
                                  const ThreadNameType& name, int stackSize) {
    if (!function) throw std::invalid_argument("CreateThread: null entry point");
    if (stackSize < -1) throw std::invalid_argument("CreateThread: invalid stack size");
    auto handle = std::make_unique<NativeThread>();
    auto args = std::make_unique<StartArgs>(StartArgs{function, parameter, name.substr(0, 15)});
    pthread_attr_t attributes;
    int error = pthread_attr_init(&attributes);
    if (error != 0) fail("pthread_attr_init", error);
    if (stackSize > 0) {
        error = pthread_attr_setstacksize(&attributes, static_cast<std::size_t>(stackSize));
        if (error != 0) {
            (void)pthread_attr_destroy(&attributes);
            fail("pthread_attr_setstacksize", error);
        }
    }
    // Release before starting: the new thread can finish before pthread_create returns.
    StartArgs* raw = args.release();
    error = pthread_create(&handle->thread, &attributes, start_thread, raw);
    (void)pthread_attr_destroy(&attributes);
    if (error != 0) {
        args.reset(raw);
        fail("pthread_create", error);
    }
    return reinterpret_cast<ThreadHandle>(handle.release());
}

mt::ThreadHandle mt::CreateThread(FuncNoArgs function, const ThreadNameType& name, int stackSize) {
    auto argument = std::make_unique<FuncNoArgs>(std::move(function));
    FuncNoArgs* raw = argument.release();
    try {
        return CreateThread(&start_function, raw, name, stackSize);
    } catch (...) {
        argument.reset(raw);
        throw;
    }
}

void mt::ThreadJoin(ThreadHandle opaque, uint32_t milliseconds) {
    auto* handle = reinterpret_cast<NativeThread*>(opaque);
    if (!handle) throw std::invalid_argument("ThreadJoin: null handle");
    if (handle->joined) return;
    int error;
    if (milliseconds == static_cast<uint32_t>(-1)) {
        error = pthread_join(handle->thread, nullptr);
    } else {
        timespec deadline{};
        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) fail("clock_gettime", errno);
        deadline.tv_sec += milliseconds / 1000;
        deadline.tv_nsec += static_cast<long>(milliseconds % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }
        error = pthread_timedjoin_np(handle->thread, nullptr, &deadline);
    }
    if (error != 0) fail("pthread_join", error);
    handle->joined = true;
}

void mt::DestroyThreadHandle(ThreadHandle opaque) {
    auto* handle = reinterpret_cast<NativeThread*>(opaque);
    if (!handle) return;
    if (!handle->joined) {
        const int error = pthread_detach(handle->thread);
        if (error != 0) fail("pthread_detach", error);
    }
    delete handle;
}

void mt::ThreadDetach(ThreadHandle handle) {
    DestroyThreadHandle(handle);
}

int mt::ThreadTerminate(ThreadHandle opaque) {
    auto* handle = reinterpret_cast<NativeThread*>(opaque);
    // Compatibility with mt.h only. Lab3 never forcibly cancels workers.
    if (!handle) return EINVAL;
    if (handle->joined) {
        delete handle;
        return 0;
    }
    int error = pthread_cancel(handle->thread);
    if (error != 0) return error;
    error = pthread_join(handle->thread, nullptr);
    if (error != 0) return error;
    delete handle;
    return 0;
}

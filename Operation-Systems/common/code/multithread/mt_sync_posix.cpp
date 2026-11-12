#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "mt_sync.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <pthread.h>
#include <stdexcept>

namespace mt {
struct MutexHandleDummy { pthread_mutex_t value; };
struct BarrierHandleDummy { pthread_barrier_t value; };
}

namespace {
[[noreturn]] void fail(const char* operation, int error) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(error));
}
[[noreturn]] void fatal(const char* operation, int error) noexcept {
    (void)std::fprintf(stderr, "FATAL: %s: error=%d\n", operation, error);
    (void)std::fflush(stderr);
    std::_Exit(EXIT_FAILURE);
}
}

mt::MutexHandle mt::MutexCreate() {
    auto mutex = std::make_unique<MutexHandleDummy>();
    const int error = pthread_mutex_init(&mutex->value, nullptr);
    if (error != 0) fail("pthread_mutex_init", error);
    return mutex.release();
}
void mt::MutexDestroy(MutexHandle handle) noexcept {
    if (!handle) return;
    const int error = pthread_mutex_destroy(&handle->value);
    if (error != 0) fatal("pthread_mutex_destroy", error);
    delete handle;
}
void mt::MutexLock(MutexHandle handle) noexcept {
    const int error = pthread_mutex_lock(&handle->value);
    if (error != 0) fatal("pthread_mutex_lock", error);
}
void mt::MutexUnlock(MutexHandle handle) noexcept {
    const int error = pthread_mutex_unlock(&handle->value);
    if (error != 0) fatal("pthread_mutex_unlock", error);
}

mt::BarrierHandle mt::BarrierCreate(unsigned participants) {
    if (participants == 0) throw std::invalid_argument("BarrierCreate: zero participants");
    auto barrier = std::make_unique<BarrierHandleDummy>();
    const int error = pthread_barrier_init(&barrier->value, nullptr, participants);
    if (error != 0) fail("pthread_barrier_init", error);
    return barrier.release();
}
void mt::BarrierDestroy(BarrierHandle handle) noexcept {
    if (!handle) return;
    const int error = pthread_barrier_destroy(&handle->value);
    if (error != 0) fatal("pthread_barrier_destroy", error);
    delete handle;
}
void mt::BarrierWait(BarrierHandle handle) noexcept {
    const int error = pthread_barrier_wait(&handle->value);
    if (error != 0 && error != PTHREAD_BARRIER_SERIAL_THREAD)
        fatal("pthread_barrier_wait", error);
}

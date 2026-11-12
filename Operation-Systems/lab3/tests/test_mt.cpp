#include "multithread/mt_sync.h"
#include "multithread/thread.h"
#include <cstdio>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void set_value(void* opaque) { *static_cast<int*>(opaque) = 42; }
struct Gate { mt::MutexHandle mutex; int value = 0; };
void wait_gate(void* opaque) {
    auto& gate = *static_cast<Gate*>(opaque);
    mt::MutexLock(gate.mutex);
    gate.value = 7;
    mt::MutexUnlock(gate.mutex);
}
struct Rendezvous { mt::BarrierHandle barrier; int value = 0; };
void rendezvous(void* opaque) {
    auto& state = *static_cast<Rendezvous*>(opaque);
    state.value = 9;
    mt::BarrierWait(state.barrier);
}
}

int main() {
    int value = 0;
    auto handle = mt::CreateThread(&set_value, &value, "test-raw");
    mt::ThreadJoin(handle);
    mt::ThreadJoin(handle); // repeated join is a no-op in this Linux backend
    require(value == 42, "raw callback failed");
    mt::DestroyThreadHandle(handle);

    value = 0;
    handle = mt::CreateThread(mt::FuncNoArgs([&value] { value = 11; }), "test-lambda");
    mt::ThreadJoin(handle);
    require(value == 11, "function callback failed");
    mt::DestroyThreadHandle(handle);

    value = 0;
    {
        mt::Thread thread("test-class", &set_value);
        thread.Run(&value);
        thread.Join();
    }
    require(value == 42, "Thread class failed");

    Gate gate{mt::MutexCreate()};
    mt::MutexLock(gate.mutex);
    handle = mt::CreateThread(&wait_gate, &gate, "test-timeout");
    bool timed_out = false;
    try { mt::ThreadJoin(handle, 1); }
    catch (const std::runtime_error&) { timed_out = true; }
    mt::MutexUnlock(gate.mutex);
    mt::ThreadJoin(handle);
    mt::DestroyThreadHandle(handle);
    require(timed_out && gate.value == 7, "timed join or mutex failed");
    mt::MutexDestroy(gate.mutex);

    Rendezvous state{mt::BarrierCreate(2)};
    handle = mt::CreateThread(&rendezvous, &state, "test-barrier");
    mt::BarrierWait(state.barrier);
    require(state.value == 9, "barrier publication failed");
    mt::ThreadJoin(handle);
    mt::DestroyThreadHandle(handle);
    mt::BarrierDestroy(state.barrier);

    bool failed = false;
    try { (void)mt::CreateThread(static_cast<mt::ThreadFunc>(nullptr)); }
    catch (const std::invalid_argument&) { failed = true; }
    require(failed, "null callback must fail");
    failed = false;
    try { (void)mt::CreateThread(&set_value, &value, "", -2); }
    catch (const std::invalid_argument&) { failed = true; }
    require(failed, "invalid stack size must fail");
    failed = false;
    try { (void)mt::BarrierCreate(0); }
    catch (const std::invalid_argument&) { failed = true; }
    require(failed, "zero barrier participants must fail");
    std::puts("mt API checks passed");
}

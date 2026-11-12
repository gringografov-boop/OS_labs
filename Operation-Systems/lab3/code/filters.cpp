#include "filters.hpp"
#include <cerrno>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include "multithread/mt.h"
#include "multithread/mt_sync.h"
#include <exception>
#include <unistd.h>
#include <utility>

namespace {
class Mutex {
    mt::MutexHandle handle_ = mt::MutexCreate();
public:
    Mutex() = default;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    ~Mutex() { mt::MutexDestroy(handle_); }
    void lock() noexcept { mt::MutexLock(handle_); }
    void unlock() noexcept { mt::MutexUnlock(handle_); }
};

class Barrier {
    mt::BarrierHandle handle_;
public:
    explicit Barrier(unsigned participants) : handle_(mt::BarrierCreate(participants)) {}
    Barrier(const Barrier&) = delete;
    Barrier& operator=(const Barrier&) = delete;
    ~Barrier() { mt::BarrierDestroy(handle_); }
    void wait() noexcept { mt::BarrierWait(handle_); }
};

struct Context {
    Matrix erosion[2];
    Matrix dilation[2];
    int iterations;
    int radius;
    bool verbose;
    bool abort_start = false;
    Barrier barrier;
    Mutex gate;
    Mutex log;
    Context(int k, int r, int p, bool v)
        : iterations(k), radius(r), verbose(v), barrier(static_cast<unsigned>(p)) {}
};

struct Task {
    Context* context;
    int id;
    int first;
    int last;
};

void log_worker(Context& context, const Task& task, const char* event) noexcept {
    char line[256];
    const int n = std::snprintf(line, sizeof line,
        "%s PID=%ld TID=%ld worker=%d rows=[%d,%d)\n", event,
        static_cast<long>(::getpid()), linux_tid(), task.id, task.first, task.last);
    if (n < 0 || static_cast<std::size_t>(n) >= sizeof line)
        fatal_posix("snprintf(log)", EIO);
    context.log.lock();
    write_stdout(line, static_cast<std::size_t>(n));
    context.log.unlock();
}

void worker(void* opaque) noexcept {
    const Task& task = *static_cast<Task*>(opaque);
    Context& c = *task.context;
    c.gate.lock();
    const bool abort = c.abort_start;
    c.gate.unlock();
    if (abort) return;

    if (c.verbose) log_worker(c, task, "START");
    const int rows = c.erosion[0].rows;
    const int cols = c.erosion[0].cols;
    for (int step = 0; step < c.iterations; ++step) {
        const int src = step % 2;
        const int dst = 1 - src;
        for (int i = task.first; i < task.last; ++i) {
            const int top = i > c.radius ? i - c.radius : 0;
            const int bottom = c.radius < rows - 1 - i ? i + c.radius : rows - 1;
            for (int j = 0; j < cols; ++j) {
                const int left = j > c.radius ? j - c.radius : 0;
                const int right = c.radius < cols - 1 - j ? j + c.radius : cols - 1;
                double low = std::numeric_limits<double>::infinity();
                double high = -std::numeric_limits<double>::infinity();
                for (int x = top; x <= bottom; ++x) {
                    const std::size_t base = static_cast<std::size_t>(x) * cols;
                    for (int y = left; y <= right; ++y) {
                        const double e = c.erosion[src].values[base + y];
                        const double d = c.dilation[src].values[base + y];
                        if (e < low) low = e;
                        if (d > high) high = d;
                    }
                }
                const std::size_t index = static_cast<std::size_t>(i) * cols + j;
                c.erosion[dst].values[index] = low;
                c.dilation[dst].values[index] = high;
            }
        }
        c.barrier.wait();
    }
    if (c.verbose) log_worker(c, task, "DONE");
}
}

Result run_filters(Matrix input, const Options& options) {
    if (options.iterations == 0) {
        Matrix dilation = copy_matrix(input);
        return {std::move(input), std::move(dilation), 0.0, 1};
    }
    const int p = options.threads < input.rows ? options.threads : input.rows;
    Context c(options.iterations, options.window / 2, p, options.verbose);
    c.erosion[0] = std::move(input);
    c.erosion[1] = Matrix(c.erosion[0].rows, c.erosion[0].cols);
    c.dilation[0] = copy_matrix(c.erosion[0]);
    c.dilation[1] = Matrix(c.erosion[0].rows, c.erosion[0].cols);
    std::unique_ptr<Task[]> tasks(new Task[p]);
    std::unique_ptr<mt::ThreadHandle[]> handles(new mt::ThreadHandle[p - 1]{});
    for (int i = 0; i < p; ++i) {
        tasks[i] = {&c, i,
            static_cast<int>(static_cast<long long>(c.erosion[0].rows) * i / p),
            static_cast<int>(static_cast<long long>(c.erosion[0].rows) * (i + 1) / p)};
    }

    const double start = monotonic_seconds();
    c.gate.lock();
    int created = 0;
    std::exception_ptr create_error;
    try {
        for (int i = 1; i < p; ++i) {
            handles[created] = mt::CreateThread(&worker, &tasks[i],
                                                "lab3-" + std::to_string(i));
            if (!handles[created])
                throw std::runtime_error("mt::CreateThread returned a null handle");
            ++created;
        }
    } catch (...) {
        // No worker may enter the barrier unless all participants were created.
        c.abort_start = true;
        create_error = std::current_exception();
    }
    c.gate.unlock();
    if (!create_error) worker(&tasks[0]);
    for (int i = 0; i < created; ++i) {
        try {
            mt::ThreadJoin(handles[i]);
            mt::DestroyThreadHandle(handles[i]);
            handles[i] = nullptr;
        } catch (...) {
            // Context/task storage must not be freed while a worker can access it.
            fatal_posix("mt::ThreadJoin/DestroyThreadHandle", EIO);
        }
    }
    if (create_error) std::rethrow_exception(create_error);
    const double seconds = monotonic_seconds() - start;

    const int index = options.iterations % 2;
    return {std::move(c.erosion[index]), std::move(c.dilation[index]), seconds, p};
}

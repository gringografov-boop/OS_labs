#include "filters.hpp"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <getopt.h>
#include <new>
#include <stdexcept>
#include <unistd.h>
#include <utility>

namespace {
const char usage[] =
    "Usage: lab3 -i FILE [-t P] [-k K] [-w W] [-o PREFIX] [-v]\n"
    "  -t P  maximum threads INCLUDING main (default 1)\n"
    "  -k K  repetitions of each independent filter (default 1)\n"
    "  -w W  positive odd square window size (default 3)\n"
    "  -o S  output prefix (default result)\n"
    "  -v    PID/TID and row range for each participant\n";

void ignore_sigpipe() {
    struct sigaction action{};
    action.sa_handler = SIG_IGN;
    if (sigemptyset(&action.sa_mask) != 0) posix_error("sigemptyset", errno);
    if (sigaction(SIGPIPE, &action, nullptr) != 0) posix_error("sigaction", errno);
}

Options parse_options(int argc, char** argv) {
    Options o;
    opterr = 0;
    int flag;
    while ((flag = getopt(argc, argv, "i:t:k:w:o:vh")) != -1) {
        switch (flag) {
        case 'i': o.input = optarg; break;
        case 't': o.threads = parse_integer(optarg, 1, "thread count"); break;
        case 'k': o.iterations = parse_integer(optarg, 0, "iteration count"); break;
        case 'w': o.window = parse_integer(optarg, 1, "window size"); break;
        case 'o': o.prefix = optarg; break;
        case 'v': o.verbose = true; break;
        case 'h': write_stdout(usage, sizeof usage - 1); std::exit(EXIT_SUCCESS);
        default: throw std::runtime_error("Unknown option or missing option value");
        }
    }
    if (o.input.empty() || o.prefix.empty() || optind != argc)
        throw std::runtime_error("Use -i FILE and a nonempty output prefix; no positional arguments");
    if (o.window % 2 == 0) throw std::runtime_error("Window must be odd");
    return o;
}
}

int main(int argc, char** argv) {
    try {
        ignore_sigpipe();
        const Options options = parse_options(argc, argv);
        Matrix input = read_matrix(options.input);

        const int actual = options.iterations == 0 ? 1 :
            (options.threads < input.rows ? options.threads : input.rows);
        
        char line[256];

        int n = std::snprintf(line, sizeof line,
            "PID=%ld TID=%ld ROWS=%d COLS=%d THREADS=%d K=%d W=%d\n",
            static_cast<long>(::getpid()), linux_tid(), input.rows, input.cols,
            actual, options.iterations, options.window);

        if (n < 0 || static_cast<std::size_t>(n) >= sizeof line)
            throw std::runtime_error("Cannot format metadata");

        write_stdout(line, static_cast<std::size_t>(n));
        Result result = run_filters(std::move(input), options);

        write_matrix(options.prefix + "_erosion.txt", result.erosion);
        write_matrix(options.prefix + "_dilation.txt", result.dilation);

        n = std::snprintf(line, sizeof line, "TIME_SECONDS=%.9f THREADS=%d\n",
                          result.seconds, result.threads);

        if (n < 0 || static_cast<std::size_t>(n) >= sizeof line)
            throw std::runtime_error("Cannot format timing");

        write_stdout(line, static_cast<std::size_t>(n));
        return EXIT_SUCCESS;
    } catch (const std::bad_alloc&) {
        const char message[] = "ERROR: Not enough memory (allocation failed)\n";
        if (::write(STDERR_FILENO, message, sizeof message - 1) < 0)
            return EXIT_FAILURE;
    } catch (const std::exception& e) {
        if (std::fprintf(stderr, "ERROR: %s\n", e.what()) < 0)
            return EXIT_FAILURE;
        if (std::fflush(stderr) != 0) return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
}

#include "common.hpp"
#include "protocol.hpp"

#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: child <output-file>\n";
        return 1;
    }

    std::ofstream output(argv[1], std::ios::out | std::ios::trunc);
    if (!output) {
        std::cerr << "Cannot open output file\n";
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        const auto parsed = parse_int_strict(line);
        if (!parsed) {
            std::cout << protocol::invalid << std::flush;
            continue;
        }

        const int value = *parsed;
        if (value < 0 || is_prime(value)) {
            std::cout << protocol::stop << std::flush;
            return 0;
        }
        if (value < 2) {
            std::cout << protocol::ignored << std::flush;
            continue;
        }

        output << value << '\n' << std::flush;
        if (!output) {
            std::cerr << "Cannot write to output file\n";
            return 1;
        }
        std::cout << protocol::written << std::flush;
    }
    return 0;
}

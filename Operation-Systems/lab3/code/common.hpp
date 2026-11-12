#pragma once
#include <cstddef>
#include <memory>
#include <string>

struct Options {
    std::string input;
    std::string prefix = "result";
    int threads = 1;
    int iterations = 1;
    int window = 3;
    bool verbose = false;
};

int parse_integer(const char* text, int minimum, const char* name);

[[noreturn]] void posix_error(const char* operation, int code);
[[noreturn]] void fatal_posix(const char* operation, int code) noexcept;

void write_stdout(const char* text, std::size_t count) noexcept;

double monotonic_seconds();
long linux_tid() noexcept;

struct Matrix {
    int rows = 0;
    int cols = 0;
    std::unique_ptr<double[]> values;

    Matrix() = default;
    Matrix(int height, int width);
    Matrix(Matrix&&) noexcept = default;
    
    Matrix& operator=(Matrix&&) noexcept = default;
    Matrix(const Matrix&) = delete;
    Matrix& operator=(const Matrix&) = delete;
    std::size_t count() const noexcept;
};

Matrix read_matrix(const std::string& path);
void write_matrix(const std::string& path, const Matrix& matrix);
Matrix copy_matrix(const Matrix& matrix);

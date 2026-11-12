#include "common.hpp"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
class File {
    FILE* file_;
public:
    File(const std::string& path, const char* mode) : file_(std::fopen(path.c_str(), mode)) {
        if (!file_) posix_error("fopen", errno);
    }
    ~File() {
        if (file_ && std::fclose(file_) != 0) fatal_posix("fclose(cleanup)", errno);
    }
    FILE* get() const noexcept { return file_; }
    void close() {
        FILE* temporary = file_;
        file_ = nullptr; // fclose consumes the stream even on failure.
        if (std::fclose(temporary) != 0) posix_error("fclose", errno);
    }
};

bool token(FILE* file, char (&buffer)[256]) {
    const int result = std::fscanf(file, "%255s", buffer);
    if (result == EOF) {
        if (std::ferror(file)) posix_error("fscanf", errno ? errno : EIO);
        return false;
    }
    if (result != 1) throw std::runtime_error("Input token cannot be read");
    if (std::strlen(buffer) == 255) throw std::runtime_error("Input token too long");
    return true;
}
}

Matrix::Matrix(int height, int width) : rows(height), cols(width) {
    if (rows <= 0 || cols <= 0) throw std::runtime_error("Invalid matrix dimensions");
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (static_cast<std::size_t>(rows) > maximum / static_cast<std::size_t>(cols) ||
        count() > maximum / sizeof(double))
        throw std::runtime_error("Matrix size overflow");
    values.reset(new double[count()]);
}

std::size_t Matrix::count() const noexcept {
    return static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
}

Matrix copy_matrix(const Matrix& matrix) {
    Matrix result(matrix.rows, matrix.cols);
    std::memcpy(result.values.get(), matrix.values.get(), matrix.count() * sizeof(double));
    return result;
}

Matrix read_matrix(const std::string& path) {
    File file(path, "r");
    char buffer[256];
    if (!token(file.get(), buffer)) throw std::runtime_error("Missing row count");
    const int rows = parse_integer(buffer, 1, "row count");
    if (!token(file.get(), buffer)) throw std::runtime_error("Missing column count");
    const int cols = parse_integer(buffer, 1, "column count");
    Matrix matrix(rows, cols);
    for (std::size_t i = 0; i < matrix.count(); ++i) {
        if (!token(file.get(), buffer)) throw std::runtime_error("Too few matrix values");
        char* end = nullptr;
        errno = 0;
        const double value = std::strtod(buffer, &end);
        if (errno != 0 || end == buffer || *end != '\0' || !std::isfinite(value))
            throw std::runtime_error("Invalid finite double in matrix");
        matrix.values[i] = value;
    }
    if (token(file.get(), buffer)) throw std::runtime_error("Extra data after matrix");
    file.close();
    return matrix;
}

void write_matrix(const std::string& path, const Matrix& matrix) {
    File file(path, "w");
    if (std::fprintf(file.get(), "%d %d\n", matrix.rows, matrix.cols) < 0)
        posix_error("fprintf(header)", errno ? errno : EIO);
    for (int i = 0; i < matrix.rows; ++i) {
        for (int j = 0; j < matrix.cols; ++j) {
            const std::size_t index = static_cast<std::size_t>(i) * matrix.cols + j;
            if (std::fprintf(file.get(), "%.17g%c", matrix.values[index],
                             j + 1 == matrix.cols ? '\n' : ' ') < 0)
                posix_error("fprintf(matrix)", errno ? errno : EIO);
        }
    }
    file.close();
}
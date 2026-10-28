#pragma once
#include "os/os.h"

class UniqueFile {
public:
    explicit UniqueFile(os::FileHandle file = nullptr) noexcept : file_(file) {}
    UniqueFile(const UniqueFile&) = delete;
    UniqueFile& operator=(const UniqueFile&) = delete;
    ~UniqueFile() { reset(); }
    os::FileHandle get() const noexcept { return file_; }
    void reset(os::FileHandle replacement = nullptr) noexcept {
        if (file_ != replacement) { os::CloseFile(file_); file_ = replacement; }
    }
private:
    os::FileHandle file_;
};

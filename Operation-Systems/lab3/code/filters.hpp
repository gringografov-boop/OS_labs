#pragma once
#include "common.hpp"

struct Result {
    Matrix erosion;
    Matrix dilation;
    double seconds = 0;
    int threads = 1;
};

Result run_filters(Matrix input, const Options& options);

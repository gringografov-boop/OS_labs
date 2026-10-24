#pragma once

#include <optional>
#include <string>

bool is_prime(int value);
std::optional<int> parse_int_strict(const std::string& text);

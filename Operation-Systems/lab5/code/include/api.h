#pragma once
#ifdef _WIN32
#ifdef LAB5_EXPORT
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif
#else
#define API __attribute__((visibility("default")))
#endif
extern "C" {
API float Derivative(float A, float deltaX);
API float Square(float A, float B);
}

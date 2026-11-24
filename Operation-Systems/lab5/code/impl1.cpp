#include "api.h"
#include <cmath>
extern "C" float Derivative(float a, float h) {
    const double x=a, d=h;
    return static_cast<float>((std::cos(x+d)-std::cos(x))/d);
}
extern "C" float Square(float a, float b) { return a*b; }

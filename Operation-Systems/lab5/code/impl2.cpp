#include "api.h"
#include <cmath>
extern "C" float Derivative(float a, float h) {
    const double x=a, d=h;
    return static_cast<float>((std::cos(x+d)-std::cos(x-d))/(2*d));
}
extern "C" float Square(float a, float b) {
    return static_cast<float>(static_cast<double>(a)*b/2);
}

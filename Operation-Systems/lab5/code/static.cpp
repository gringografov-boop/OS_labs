#include "api.h"
#include "dialog.h"
int main() {
    Function derivative=&Derivative, square=&Square;
    return Dialogue(derivative,square,[]{std::cerr<<"Switch is available only in program 2\n";});
}

# Lab 5, variant 13

Contracts: Derivative(float A, float deltaX), Square(float A, float B).
Implementation 1: forward difference of cos(x); rectangle area.
Implementation 2: central difference of cos(x); right-triangle area (A,B are legs).

lab5_static is program 1: link-time dependency on the SHARED lab5_impl1 library.
Despite the executable name, the function implementation is not statically linked.
lab5_dynamic is program 2: no linkage to either implementation; OS loading and symbol lookup.
Paths are formed relative to the executable directory, not hardcoded to the build folder.
Command 0 loads the other implementation, validates both symbols, swaps pointers and unloads the old one.
On load failure the program reports the error and returns nonzero.

Commands: 1 A deltaX; 2 A B; 0 (program 2 only). EOF exits.
Angles are radians. deltaX must be nonzero; sides must be positive.
The exported API returns float; derivative intermediate calculations use double.

Build from Operation-Systems:
cmake -S . -B build-lab5
cmake --build build-lab5 --config Release --target lab5_static lab5_dynamic
python lab5/tests/test_lab5.py build-lab5/bin/lab5

Binaries and shared libraries: build-lab5/bin/lab5 for all configurations.

Analysis: program 1 has compiler-checked function declarations and a loader-resolved
startup dependency. Replacing its contract requires rebuilding the client.
Program 2 selects implementations at runtime, but must check load and lookup errors;
it must not call function pointers after unloading the library. Both methods use
dynamic libraries; link-time linkage must not be confused with static libraries.

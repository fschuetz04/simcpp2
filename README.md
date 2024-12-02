# SimCpp20

[![Linux](https://github.com/fschuetz04/simcpp20/actions/workflows/linux.yml/badge.svg)](https://github.com/fschuetz04/simcpp20/actions/workflows/linux.yml)
[![Windows](https://github.com/fschuetz04/simcpp20/actions/workflows/windows.yml/badge.svg)](https://github.com/fschuetz04/simcpp20/actions/workflows/windows.yml)

SimCpp20 is a discrete-event simulation framework for C++20.
It is similar to SimPy and aims to be easy to set up and use.

Processes are defined as functions receiving `simcpp20::simulation<> &` as their first argument and returning `simcpp20::event<>`.
Each process is executed as a coroutine.
Thus, this framework requires C++20.
A short example simulating two clocks ticking in different time intervals looks like this:

```c++
#include <cstdio>

#include "fschuetz04/simcpp20.hpp"

simcpp20::event<> clock_proc(simcpp20::simulation<> &sim, char const *name,
                             double delay) {
  while (true) {
    printf("[%.0f] %s\n", sim.now(), name);
    co_await sim.timeout(delay);
  }
}

int main() {
  simcpp20::simulation<> sim;
  clock_proc(sim, "slow", 2);
  clock_proc(sim, "fast", 1);
  sim.run_until(5);
}
```

When run, the following output is generated:

```text
[0] slow
[0] fast
[1] fast
[2] slow
[2] fast
[3] fast
[4] slow
[4] fast
```

Other examples can be found in the `examples/` folder.

This project uses CMake.
To build and execute the clocks example, run the following commands:

```shell
cmake -B build
cmake --build build
build/examples/clocks
```

The CMake configuration has been tested with GCC (version 10 or later), Clang (version 14 or later) and MSVC.
If such a version is available under a different name (for example `g++-10`), you can try `CXX=g++-10 cmake ..` instead of just `cmake ..` to set the C++ compiler command.
When using an MSVC compiler, it must be of version 19.28 or later (Visual Studio 2019 version 16.8 or later).
Contributions to improve compiler support are welcome!

If you want to use SimCpp20 in your project, the easiest option is to use CMake with FetchContent.
A simple configuration looks like this:

```cmake
cmake_minimum_required(VERSION 3.14)

project(my_app)

include(FetchContent)

FetchContent_Declare(fschuetz04_simcpp20
    GIT_REPOSITORY https://github.com/fschuetz04/simcpp20
    GIT_TAG        ad975317db40d81dbe4c45c5db4edcd8ec6e2de8) # replace with latest revision

FetchContent_MakeAvailable(fschuetz04_simcpp20)

add_executable(app app.cpp)
target_link_libraries(app PRIVATE fschuetz04::simcpp20)
```

Replace the commit hash with the latest commit hash of SimCpp20 accordingly.

## Copyright and License

Copyright © 2024 Felix Schütz.

Licensed under the MIT License.
See the `LICENSE` file for details.

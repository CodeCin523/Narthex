# Narthex

A C foundation library. It gives infrastructure building blocks to applications,
engines, and simulations.

> **Status: early development.** Nothing is stable. The API changes without notice.
>  The `vulkan` module is a stub. Do not depend on this yet.

## At a glance

- Public prefix `nth_` for functions, `Nth` for types.
- Public headers are **C99**. The library itself builds as **C17**.
- Builds a shared library `libnarthex.so`, exported as `narthex::narthex`.
- Requires CMake 3.23 or later. The library links only libc today.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Options

| Option | Default | What it does |
|---|---|---|
| `NTH_SHARED` | `ON` | Build `libnarthex.so` / `narthex.dll`. Off builds `libnarthex.a`. |
| `NTH_BUILD_TESTS` | `OFF` | Build the test suite and enable CTest. |
| `NTH_ENABLE_vulkan` | `ON` | Vulkan render backend. |
| `NTH_POISON` | `OFF` | Fill discarded arena memory with `0xDD`, fresh allocations with `0xCD`. |
| `NTH_ASAN` | `OFF` | Build with AddressSanitizer. |
| `NTH_UBSAN` | `OFF` | Build with UndefinedBehaviorSanitizer. Not available on MSVC. |

Module dependencies resolve on their own: enabling a module enables everything it
declares in `DEPENDS`. No module declares one today.

A module keeps its public header beside its source, in
`modules/<name>/include/narthex/nth_<name>.h`, and only enabled modules install
theirs. The installed tree merges into a single `narthex/` directory either way, so
consumers always write `#include <narthex/nth_vulkan.h>` and never see the split.
A disabled module leaves no header behind to include by mistake.

`NTH_ASAN` and `NTH_UBSAN` apply to the library, the modules, and the tests
together, because sanitizers need consistent instrumentation across the whole
binary. Prefer these options over passing `-fsanitize=...` in `CMAKE_C_FLAGS`: on
MSVC, enabling AddressSanitizer means *removing* flags that CMake injects, which
you cannot do by adding flags.

Consumers linking `narthex::narthex` must use the same sanitizer flags the library was
built with, or the link fails on missing sanitizer runtime symbols.

## Symbol visibility

A shared build compiles with `-fvisibility=hidden` and exports only what is marked
`NTH_API` in the public headers. Nothing else leaves the library, so the internal
functions are unreachable from outside even though they have external linkage.

`NTH_API` lives in `<narthex/utils/api.h>` and resolves against `NTH_SHARED`, which
`nth_config.h` records at configure time. That is what keeps a consumer of a static
build from being handed `__declspec(dllimport)`, and it means the header set matches
the artifact it was installed beside. `NTH_API` is empty in a static build.

Declaring a new public function means marking it `NTH_API`. Leaving it off builds
and links fine in a static build and fails at link time in a shared one.

## Tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNTH_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests live in `tests/<domain>/`, one file per test, named for what they are:

```sh
ctest --test-dir build -L unit          # unit_*   no hardware needed
ctest --test-dir build -L integration   # test_*   needs a GPU
ctest --test-dir build -L bench         # bench_*  comparison report, no pass/fail
```

To run the suite under the sanitizers:

```sh
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug \
      -DNTH_BUILD_TESTS=ON -DNTH_ASAN=ON -DNTH_UBSAN=ON
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

With `NTH_ASAN=ON` the arenas report their own memory to AddressSanitizer, so a
read or write to a block discarded by `restore` or `clean` faults at the offending
line. Detection is exact for 8-byte-aligned allocations and loses up to 7 bytes of
precision at the edges of unaligned ones.

## Use from CMake

```cmake
find_package(Narthex REQUIRED COMPONENTS vulkan)
target_link_libraries(my_app PRIVATE narthex::narthex)
```

The component check fails if the requested module was not enabled in the build you
are linking against.

## Layout

| Path | Contents |
|---|---|
| `include/narthex/` | Public API of the base modules. `utils/`, `inl/`, `mem/`, and the umbrella headers. |
| `src/` | The library itself, always compiled. One target, one `CMakeLists.txt`. |
| `modules/<name>/` | Optional modules, declared in `modules.cmake`. Public header in `modules/<name>/include/narthex/`. |
| `cmake/` | Module system, config generation, install rules. |
| `tests/` | CTest suites. |

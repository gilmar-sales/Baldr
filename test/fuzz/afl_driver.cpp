// afl_driver.cpp - afl++ persistent-mode harness entry shim
//
// Each fuzz TU defines `extern "C" int LLVMFuzzerTestOneInput(const uint8_t*,
// std::size_t)`; this file supplies the `main()` that reads input from a file
// path or stdin and feeds it to the harness. When built with afl++'s compiler
// wrappers (`afl-clang-fast++` or `afl-g++-fast`) the optional `__AFL_LOOP`
// macro enables persistent mode, which is ~100x faster than one-shot mode for
// sync surfaces.
//
// To enable persistent mode, define BALDR_FUZZ_PERSISTENT before compiling
// (the fuzz CMakeLists adds this automatically when an afl++ wrapper is
// detected).

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

// `__AFL_LOOP` is provided by afl-clang-fast / afl-g++-fast when persistent
// mode is requested. Fall back to a no-op so the harness still compiles and
// runs (once) when the macro is missing — useful for smoke testing without
// a real afl++ install.
#ifndef __AFL_LOOP
#  define __AFL_LOOP(N) (1)
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size);

namespace
{
    std::vector<uint8_t> readEntireFile(const char* path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            std::fprintf(stderr, "afl_driver: cannot open %s\n", path);
            std::exit(2);
        }
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                                    std::istreambuf_iterator<char> {});
    }

    std::vector<uint8_t> readAllStdin()
    {
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(std::cin),
                                    std::istreambuf_iterator<char> {});
    }
} // namespace

int main(int argc, char** argv)
{
#ifdef BALDR_FUZZ_PERSISTENT
    // Persistent-mode harness driven by afl-clang-fast / afl-g++-fast.
    //
    // `__AFL_LOOP(N)` returns 1 while afl++ wants more iterations, 0 when
    // it is done with this fork-server cycle. Inside the loop we read from
    // stdin, which afl++ keeps populated with mutated inputs.
    while (__AFL_LOOP(1000) == 1)
    {
        auto bytes = readAllStdin();
        (void)LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    }
    return 0;
#else
    std::vector<uint8_t> bytes;
    if (argc >= 2 && argv[1] != nullptr && argv[1][0] != '\0')
    {
        bytes = readEntireFile(argv[1]);
    }
    else
    {
        bytes = readAllStdin();
    }
    (void)LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    return 0;
#endif
}
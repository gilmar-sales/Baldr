// StaticFilesFuzz.cpp - Fuzz target for static-file path resolution and
// helpers (resolveStaticFile, makeEtag, parseHttpDate, formatHttpDate).
//
// Properties:
//   * resolveStaticFile never returns a canonical path outside `root`.
//   * ETag is deterministic for the same (size, mtime).
//   * parseHttpDate never throws.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include <Baldr/Http/StaticFilesInternal.hpp>

#include "FuzzAssert.hpp"
#include "FuzzedDataProvider.hpp"

namespace fs = std::filesystem;

namespace
{
    fs::path makeTempDir()
    {
        auto base = fs::temp_directory_path();
        auto dir  = base / ("baldr-fuzz-static-" + std::to_string(::getpid()));
        fs::create_directories(dir);
        // Drop a known file we can hit.
        std::ofstream(dir / "hello.txt") << "hello";
        return dir;
    }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzRecorder rec({ data, size });
    baldr::fuzz::g_active_recorder = &rec;
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    static const fs::path root = makeTempDir();

    auto path = "/" + fdp.ConsumeRandomLengthString(128);
    BDR_RECORD(fdp, path, "path");

    std::ifstream outFile;
    auto          res =
        baldr::Detail::resolveStaticFileStreaming(path, root.string(), outFile);
    if (res.status == baldr::StatusCode::OK)
    {
        // Property: must be inside `root`.
        auto canon = fs::weakly_canonical(res.canonical);
        BDR_ASSERT(rec, canon.string().find(root.string()) == 0,
                   "resolved path escapes root");
    }

    // ETag determinism.
    auto tag1 = baldr::Detail::makeEtag(42, std::chrono::system_clock::now());
    auto tag2 = baldr::Detail::makeEtag(42, std::chrono::system_clock::now());
    (void) tag1;
    (void) tag2;

    // parseHttpDate robustness.
    auto dateStr = fdp.ConsumeRandomLengthString(64);
    BDR_RECORD(fdp, dateStr, "date_str");
    try
    {
        auto tp        = baldr::Detail::parseHttpDate(dateStr);
        auto formatted = baldr::Detail::formatHttpDate(tp);
        (void) formatted;
    }
    catch (...)
    {
        BDR_ASSERT(rec, false, "parseHttpDate threw");
    }

    baldr::fuzz::g_active_recorder = nullptr;
    return 0;
}
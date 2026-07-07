# Fuzzing Baldr

Baldr ships a release-gated fuzz lane that exercises the web stack against
the implementation gaps called out in
[`.kilo/plans/1783003792736-webapp-gap-analysis.md`](../.kilo/plans/1783003792736-webapp-gap-analysis.md).
The lane is gated by `BALDR_BUILD_FUZZING=ON` and wired into ctest; release
CI runs every harness for a bounded time window under afl++ and fails the
release when a crash, hang, or sanitizer violation is found.

This page covers how to run the fuzzers locally, what each harness targets,
and how to debug a regression repro.

## Engines and sanitizers

- **Engine:** [afl++](https://github.com/AFLplusplus/AFLplusplus). The
  harnesses are written against the standard
  `LLVMFuzzerTestOneInput(const uint8_t*, std::size_t)` entry point and use
  a shared `afl_driver.cpp` shim that supports afl++'s `__AFL_LOOP` persistent
  mode (≈100× faster than one-shot mode for sync surfaces).
- **Sanitizers in the release-gating lane:** ASan + UBSan (gcc-16). MSan
  runs nightly on clang-18; TSan runs as deterministic-replay of the corpus
  harvested by the gating lane.

## Quickstart

Install afl++ (Ubuntu):

```bash
sudo apt-get install -y afl++
```

Configure and build with fuzzing enabled:

```bash
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBALDR_BUILD_FUZZING=ON
cmake --build build
```

Run a single harness under ctest (90 s bound):

```bash
ctest --test-dir build --output-on-failure -R fuzz.HttpParserFuzz
```

Or invoke the harness directly through afl-fuzz for an interactive session:

```bash
mkdir -p out
afl-fuzz -i test/fuzz/corpus/parser \
          -o out \
          -M parser \
          -- ./build/test/fuzz/HttpParserFuzz
```

## Adding a new harness

1. Add a new `<Name>Fuzz.cpp` to `test/fuzz/` with an
   `extern "C" int LLVMFuzzerTestOneInput(...)` definition.
2. Append the file to `BALDR_FUZZ_SOURCES` in `test/fuzz/CMakeLists.txt`.
3. Drop at least one seed file under `test/fuzz/corpus/<name>/`.
4. Add a property assertion block — see `HttpParserFuzz.cpp` for the
   pattern. Property assertions turn crashes into oracle failures.
5. Open a PR; CI runs the gating lane automatically.

## Debugging a regression

1. Find the crashing input under `test/fuzz/out/<harness>/crashes/` (or
   the upload-artifact from the CI run).
2. Reproduce with:
   ```bash
   ./build/test/fuzz/<harness> test/fuzz/out/<harness>/crashes/id_000000
   ```
3. Re-run under ASan+UBSan to capture a stack trace:
   ```bash
   cmake -S . -B build-asan \
         -DCMAKE_BUILD_TYPE=RelWithDebInfo \
         -DBALDR_BUILD_FUZZING=ON \
         -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g" \
         -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
   cmake --build build-asan --target <harness>
   ./build-asan/test/fuzz/<harness> <crash-input>
   ```
4. Once reproduced, mirror the input as a unit test inside the matching
   `test/src/<Area>/<Name>Spec.cpp` so the regression is caught by
   `ctest` going forward.

## Per-harness target matrix

| Harness | Symbol under test | Gaps |
|---|---|---|
| `HttpParserFuzz` | `HttpRequestParser::tryParse` | 1.1 keep-alive, 1.3 connection-close, 3.4 body cap, 4.1 headers |
| `RouterFuzz` *(M1)* | `Router::match`, `matchWithAllow` | 2.1 constraints/metadata, 2.3 405+Allow |
| `MiddlewareFuzz` *(M1)* | full pipeline (Cors, Csrf, Sec-headers, RateLimit, RequestId, Logging, ExceptionHandler) | 3.1, 3.2, 3.3, 3.7, 5.1, 5.2 |
| `CookieFuzz` *(M1)* | request cookie parse, response cookie emission | 3.1, 3.2 |
| `CompressionFuzz` *(M1)* | `CompressionMiddleware` | 4.2 |
| `StaticFilesFuzz` *(M1)* | `MapStaticFiles` path resolution + options | 4.1, 4.3, 4.4 |
| `ResultsFuzz` *(M1)* | `TextResult`, `JsonResult`, `ContentResult`, `StatusResult`, `FileStreamResult` | 7.1 |
| `ValidationFuzz` *(M1)* | `ValidationMiddleware` + helpers | 7.1 |
| `OpenApiSpecFuzz` *(M1)* | spec builder | 7.2 |
| `HttpServerFuzz` *(M2)* | full server, in-process loopback TCP | 1.1, 2.2, 2.3, 6.2, 8.2 |

Only `HttpParserFuzz` is shipped in the M0 release. The other harnesses
land in M1/M2 alongside the implementation gaps they target — each gap
PR is expected to add at least one corpus seed and at least one property
assertion to the relevant harness before merge.
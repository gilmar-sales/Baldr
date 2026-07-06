# Plan — Fuzzy Testing the Web Gap Plan (Release-Gated CI)

## 1. Goal

Deliver a **release-gated fuzz lane** in CI that runs before each release, exercises the Baldr web stack that the gap analysis touches (parser, router, middleware, results, static files, server), and **fails the release** when a fuzz run discovers a new crash, hang, or sanitizer violation.

Targets (from the gap analysis `1783003792736-webapp-gap-analysis.md`): `HttpRequestParser`, `Router` + middleware stack, `Results` serializers, and `HttpServer` end-to-end.

## 2. Decisions already taken

- Engine: **afl++** (user-selected).
- Sanitizer matrix: **ASan + UBSan + MSan + TSan** (user-selected; run as separate lanes).
- Integration: **separate `test/fuzz/` target wired into ctest with `-runs=N` bound** (user-selected).
- Corpus: **hand-curated seed corpus per surface**, checked in.
- Toolchain: gcc-16 (matches `cmake-multi-platform.yml`). Windows/MinGW row from `cmake-multi-platform.yml` is intentionally excluded from the fuzz lane (afl++ gcc plugin does not ship a MinGW build and Windows/MSVC is already noted in `AGENTS.md` as out-of-matrix because C++26 reflection is missing there).

## 3. Open decisions to confirm

These block M0. Recommend the listed answer; reply if you want a different one.

1. **MSan lane toolchain** — MSan needs clang; gcc cannot run MSan. Recommendation: MSan lane runs on **clang-18 nightly only**, not on the release-gating lane. Gating uses ASan+UBSan on gcc-16.
2. **afl++ install source on CI** — apt on Ubuntu 24.04 may lag. Recommendation: build from the official `AFLplusplus/AFLplusplus` release tarball pinned to a known commit, cached under `actions/cache`.
3. **TSan mode** — afl++ + TSan is unstable. Recommendation: TSan runs as a **deterministic-replay** job on the corpus harvested by the ASan lane (no live mutation under TSan).
4. **FDP (FuzzedDataProvider) source** — port `llvm::FuzzedDataProvider` (Apache-2.0 with LLVM exceptions) trimmed to the methods we use, attributed in each header. Keeps mutation semantics consistent with gtest fuzz-specs.
5. **Release trigger** — `on.release.types: [published]` plus `workflow_dispatch` for manual. PRs do **not** run the full fuzz lane; they run a 60s smoke subset. The release gate is what the user asked for.
6. **`HttpServerFuzz` in gating** — keep DISABLED in ctest by default (threaded loopback + afl++ + sanitizers is the unstable combination). It is opt-in via ctest label `fuzz_server`.

## 4. Affected boundaries

- `CMakeLists.txt` — add `option(BALDR_BUILD_FUZZING OFF)` and `add_subdirectory(test/fuzz)` only when Baldr is the top-level project (`STREQUAL` guard at `CMakeLists.txt:23`), matching the policy already used for examples/tests.
- `test/CMakeLists.txt` — unchanged. Fuzz harnesses live in a sibling `test/fuzz/` directory with its own `CMakeLists.txt`.
- `.github/workflows/` — new file `fuzz.yml`.
- New directory `test/fuzz/` with harnesses, corpus, and helper shims.
- `.gitignore` — add `test/fuzz/out/`, `test/fuzz/sync_dir/`, `test/fuzz/crashes/`, `test/fuzz/hangs/`.

No source files under `src/Baldr/` are touched. No public API changes.

## 5. Data flow

```
GitHub release published
        │
        ▼
.github/workflows/fuzz.yml
        │
        ├── job: afl (ASan+UBSan, gcc-16)        ── gates release
        ├── job: msan (clang-18)                 ── advisory, blocks release if fails
        ├── job: tsan-replay (gcc-16)            ── advisory
        └── job: coverage (gcc-16)               ── publishes report
        │
        ▼
test/fuzz/<Harness>            (afl-fuzz -i corpus -o out -- ./harness)
        │
        ▼
out/<harness>/fuzzer_stats     (parsed by workflow: unique_crashes > 0 → fail)
        │
        ▼
actions/upload-artifact        (crashes/ dir attached to the failing run)
```

## 6. Repo layout (new)

```
test/fuzz/
  CMakeLists.txt                  -- opt-in target, default OFF
  afl_driver.cpp                  -- persistent-mode entry shim
  FuzzedDataProvider.hpp          -- ported from llvm::FDP, attributed
  HttpParserFuzz.cpp              -- F1: HttpRequestParser::tryParse
  RouterFuzz.cpp                  -- F2: Router::match + matchWithAllow
  MiddlewareFuzz.cpp              -- F3: full middleware chain
  CookieFuzz.cpp                  -- F4: cookie parse + emit
  CompressionFuzz.cpp             -- F5: CompressionMiddleware (zlib)
  StaticFilesFuzz.cpp             -- F6: MapStaticFiles + path resolution
  ResultsFuzz.cpp                 -- F7: TextResult/JsonResult/StatusResult/FileStreamResult
  ValidationFuzz.cpp              -- F8: validators (Gap 7.1 surface)
  OpenApiSpecFuzz.cpp             -- F9: spec builder
  HttpServerFuzz.cpp              -- F10: loopback in-process server, DISABLED by default
  corpus/<harness>/*.bin          -- hand-curated seeds
  scripts/run_one_fuzz.sh         -- afl-fuzz wrapper with timeout + stats parsing
.github/workflows/fuzz.yml       -- release-gated + nightly + dispatch
docs/fuzzing.md                   -- operator runbook
.gitignore                        -- append fuzz output dirs
```

## 7. CMake (`test/fuzz/CMakeLists.txt`)

- Top-level `option(BALDR_BUILD_FUZZING "Build fuzz harnesses" OFF)` exposed only when `CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR`. Defaults OFF for downstream consumers (same policy as `BALDR_BUILD_EXAMPLES` / `BALDR_BUILD_TESTS`).
- One executable per harness (`HttpParserFuzz`, …). PCH **off** for harnesses (cold compile + smaller diagnostic surface). Unity build **off** per harness TU so harness-specific regressions are not masked.
- Inject `BALDR_FUZZ_CXX_FLAGS` / `BALDR_FUZZ_LINK_FLAGS` from the workflow (ASan/UBSan/MSan/TSan flavors). Default values are `-g -O1` for local dev.
- Each executable registered with `add_test(NAME fuzz.<harness> COMMAND scripts/run_one_fuzz.sh ...)`. `TIMEOUT 120` on ctest properties.
- `HttpServerFuzz` registered with `LABELS "fuzz_server"` and `DISABLED ON`; runnable via `ctest -L fuzz_server`.
- `scripts/run_one_fuzz.sh` calls `afl-fuzz -i corpus -o out -M <harness> -- ./<harness>` under `timeout --kill-after=10s 90s` (or release lane uses longer bound), parses `out/<harness>/fuzzer_stats`, exits non-zero if `unique_crashes > 0` or `unique_hangs > 0`.

`CMakeLists.txt:23-26` already has the `STREQUAL` guard — extend it with:

```cmake
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  set(BALDR_BUILD_EXAMPLES ON)
  set(BALDR_BUILD_TESTS ON)
  set(BALDR_BUILD_FUZZING ON)
endif()
```

…and an `if(BALDR_BUILD_FUZZING) add_subdirectory(test/fuzz) endif()` block mirroring the existing `BALDR_BUILD_TESTS` block at `CMakeLists.txt:103-107`.

## 8. Harness contract

Each sync harness TU defines `extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)` and links against `baldr::baldr`. `afl_driver.cpp` provides the `main()` that reads `argv[1]` or `stdin`, calls the harness, and supports `__AFL_LOOP` persistent mode.

`HttpServerFuzz` deviates: it is a standalone binary that binds to `127.0.0.1:0`, spawns N mutation iterations of raw-TCP writes against the real `HttpServer`, asserts response invariants, exits 0 on no crash. Deterministic when fed captured bytes (the corpus format for F10 is **literal HTTP/1.1 request bytes**, not FDP-shaped input).

## 9. Property assertions (oracle layer)

Implemented as inline asserts in each harness; **also** mirrored as gtest predicates inside the existing `*Spec.cpp` files so a regression repro is unit-testable without afl++.

- **F1**: `parser.readBufferIndex <= parser.bufferSize`; CL-negative rejected; `Transfer-Encoding: chunked` + CL rejected; body ≤ `maxBodySize`.
- **F2**: path miss returns either 404 or 405+`Allow:`; route params fully URL-decoded; no double-free on `extractParamsRegex`.
- **F3**: response never contains `std::exception::what()` in non-debug builds (Gap 3.7); CSRF rejects unsafe method without valid pair; rate-limit bucket does not overflow `size_t`.
- **F4**: cookie name matches RFC 6265 token grammar; quoted values preserve commas; emitted headers never contain `\r\n`.
- **F5**: compressed body length ≤ `input * 1.0001 + 64` (ratio-bomb guard); `Content-Encoding` absent below threshold.
- **F6**: resolved path always contained under `MapStaticFiles` root; ETag deterministic; out-of-range `Range:` → 416.
- **F7**: `Content-Length` matches `body.size()` byte-exact; JSON with surrogates rejected at parse.
- **F8**: validator on `NaN` does not short-circuit to `true`; int overflow → error.
- **F9**: emitted JSON round-trips through simdjson.
- **F10**: response order matches request order per connection; `Connection: close` honored within 5s; pipeline reordering never observed; WorkerPool drains on shutdown.

## 10. Seed corpus (hand-curated, checked in)

Size budget: ≤2 KB each, ≤30 files per harness.

- `parser/`: `get_minimal`, `get_with_host`, `post_chunked`, `post_cl_smaller_than_body`, `dup_content_length`, `oversized_headers`, `pipelined_2req`, `websocket_upgrade`, `rfc7230_example_1`, `binary_garbage_256B`.
- `router/`: `literal_collisions`, `param_decode_with_null`, `wildcard_star`, `method_mismatch_405`, `path_with_query`.
- `middleware/`: `csrf_token_mismatch`, `rate_limit_flood_64`, `cors_origin_with_null`, `exception_with_long_what`.
- `cookies/`: `quoted_value`, `duplicate_name`, `max_age_overflow`, `samesite_case_variants`.
- `compression/`: `accept_encoding_trick`, `body_below_threshold`, `body_blow_up`.
- `static_files/`: `traversal_dotdot`, `traversal_percent_encoded`, `range_multi`, `if_none_match_weak`.
- `results/`: `json_huge_number`, `json_with_surrogates`, `status_unknown_code`.
- `validation/`: `nan_inf`, `empty_string_required`, `out_of_range_int`.
- `openapi/`: `metadata_with_newlines`, `path_with_double_slash`.
- `server/`: `pipeline_5_keep_alive`, `half_close_then_continue`, `abrupt_reset`.

## 11. Release-gated CI (`.github/workflows/fuzz.yml`)

Triggers:

- `on: release: { types: [published] }` → release-gated (the user-requested lane).
- `on: workflow_dispatch` → manual run.
- `on: schedule: cron: '0 3 * * *'` → nightly full run.

Jobs:

| Job | Trigger | Toolchain | Sanitizer | Bound | Effect |
|---|---|---|---|---|---|
| `afl-gating` | release, dispatch | gcc-16 + afl++ (source build, cached) | ASan+UBSan | 90 s/harness | **Blocks release** |
| `msan-nightly` | nightly | clang-18 | MSan | 5 min/harness | **Blocks release** |
| `tsan-replay` | nightly, dispatch | gcc-16 | TSan (corpus replay) | replay all harvested | Advisory |
| `coverage` | weekly | gcc-16 | – | afl-coverage on harvested corpus | Posts Markdown summary |

Each job:

1. Install afl++ from pinned tarball (cached by version).
2. `cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBALDR_BUILD_FUZZING=ON -DCMAKE_CXX_FLAGS=<sanitizer flags> -DCMAKE_EXE_LINKER_FLAGS=<sanitizer flags>`.
3. `cmake --build build-fuzz --target <harnesses>` (F10 excluded from gating).
4. For each harness, run `scripts/run_one_fuzz.sh ./build-fuzz/test/fuzz/<harness> test/fuzz/corpus/<harness>`.
5. Parse `out/<harness>/fuzzer_stats` after each run; non-zero crashes → `actions/upload-artifact` of `out/<harness>/crashes/` and job failure.
6. `afl-gating` is `needs: []`; the other jobs run in parallel and report to the release via `if: github.event_name == 'release'` on the gating job.

PR lane: not in this workflow. Add a small **PR smoke** job to `cmake-multi-platform.yml` separately (out of scope for this plan — `cmake-multi-platform.yml` is documented as the *build+ctest* workflow and does not own fuzzing).

## 12. Failure modes & rollout

- **afl++ build fails on a specific Ubuntu runner image** → pin to `ubuntu-22.04` explicitly and cache the tarball by commit SHA.
- **afl++ + gcc-16 + C++26 fails to compile a harness** → fall back to `-std=c++23` *only for fuzz harness TUs* via `target_compile_options(<harness> PRIVATE -std=c++23)`; library stays c++26. Document in `docs/fuzzing.md`.
- **ASan lane produces too many crashes to triage** → triage by corpus merge from previous green run, ship only deltas.
- **MSan + clang-18 incompatible with a library transitive dep** → mark MSan lane as `continue-on-error: true` and report-only until deps are fixed.
- **F10 deadlock under TSan** → keep DISABLED in ctest; rely on the gtest concurrent-client integration test (Gap 8.2 first deliverable) instead.
- **afl++-gcc-plugin not in apt** → source-build from upstream tag, cache under `actions/cache@v4`.

## 13. Validation plan

- After M0 lands: `cmake -S . -B build -DBALDR_BUILD_FUZZING=ON && cmake --build build` succeeds; `ctest --test-dir build --output-on-failure` runs `fuzz.HttpParserFuzz` for 90s without crash on `test/fuzz/corpus/parser`.
- After M1 lands: all ten sync harnesses run in ctest; AFL gating job green on a known seed.
- After M2 lands: nightly MSan + TSan-replay jobs green; coverage report uploaded.
- Pre-release sign-off: trigger `workflow_dispatch` and confirm `afl-gating` + `msan-nightly` pass on the release SHA.

## 14. Out of scope

- Wiring fuzz into PR-time `cmake-multi-platform.yml` (separate plan if wanted).
- Adding H2/H3 fuzzers (gap analysis marks H2/H3 out of scope).
- Coverage thresholds or regressions gates (only advisory).
- Replacing afl++ with libFuzzer (libFuzzer needs clang; out of matrix for gcc-16).
- Editing any existing source under `src/Baldr/`, `examples/`, or `docs/`.

## 15. Ordered task list

1. **M0 — skeleton + first harness (gating-bound).**
   - Add `option(BALDR_BUILD_FUZZING ...)` and `add_subdirectory(test/fuzz)` to `CMakeLists.txt`.
   - Add `test/fuzz/CMakeLists.txt`, `afl_driver.cpp`, `FuzzedDataProvider.hpp`, `HttpParserFuzz.cpp`, one seed file, `scripts/run_one_fuzz.sh`.
   - Update `.gitignore` for fuzz output dirs.
   - Add `docs/fuzzing.md` (operator runbook).
   - Local validation: `cmake -S . -B build -DBALDR_BUILD_FUZZING=ON && cmake --build build && ctest --test-dir build --output-on-failure -R fuzz.HttpParserFuzz` passes 90s.

2. **M1 — sync harnesses.**
   - Add F2–F9 harnesses, seeds, property assertions (also mirrored into the existing `*Spec.cpp` files where appropriate).
   - Add `.github/workflows/fuzz.yml` with the `afl-gating` and `msan-nightly` jobs, triggered on release + dispatch + nightly.
   - Validate on a draft release: gating job must pass.

3. **M2 — async + sanitizer matrix.**
   - Add F10 `HttpServerFuzz` (DISABLED by default; runnable via ctest label).
   - Add `tsan-replay` and `coverage` jobs.
   - Validate by triggering `workflow_dispatch` and inspecting artifacts.

4. **M3 — hardening rollout (forward-only, never a discrete PR).**
   - Every implementation-gap PR must add ≥1 corpus seed + ≥1 property assertion in the relevant harness and keep `afl-gating` green.
   - Reviewer checklist (manual): corpus seed added, property asserted, gating job green for that harness.
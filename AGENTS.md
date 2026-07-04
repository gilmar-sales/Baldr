# AGENTS.md

Guidance for Kilo (and other AI coding agents) working in this repository.

## Toolchain (C++)

- CMake **3.28+**, **C++26** (`CMAKE_CXX_STANDARD 26` in `CMakeLists.txt:8`).
- CI installs **gcc-16** on Linux (better C++26 support). Windows/MSVC (`cl`) and Clang are intentionally not part of the matrix because neither implements C++26 reflection, which Baldr relies on.
- Formatting: `.clang-format` (Microsoft base, 80-col, 4-space indent, no tabs, pointers left-aligned). Match it on new code.
- External deps fetched via `FetchContent`: `trantor` (pinned `v1.5.28`, c-ares off, TLS off), `skirnir` (`v0.22.0`), `simdjson` (via skirnir), `zlib` (`v1.3.2`). Do not add system-package fallbacks unless asked.
- Unity build is enabled for both `baldr` and the test target (`UNITY_BUILD_BATCH_SIZE 4`). Watch for this if a single TU fails — pass `--target baldr -j1` or disable unity locally to bisect.
- `baldr` target uses precompiled headers including `<Baldr/Baldr.hpp>` itself; changes to that header force PCH recompilation across all examples/tests.

## Build

`CMakeLists.txt` makes examples **and** tests **default ON** when Baldr is the top-level project (see `CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR` guard at `CMakeLists.txt:11`). The options `BALDR_BUILD_EXAMPLES` / `BALDR_BUILD_TESTS` default to **OFF** for downstream consumers — don't change the defaults.

From the repo root:

```bash
cmake -S . -B build
cmake --build build --config Release
```

To skip examples (faster iteration):

```bash
cmake -S . -B build -DBALDR_BUILD_EXAMPLES=OFF
cmake --build build
```

Library target is `baldr` (alias `baldr::baldr`); public include root is `src/`, so consumers write `#include <Baldr/Baldr.hpp>`. `compile_commands.json` is exported (`.gitignored`).

## Tests

GoogleTest is fetched in `test/CMakeLists.txt` (pinned `v1.17.0`). All tests live under `test/src/*Spec.cpp` (grouped by `Http/`, `Middleware/`, `Application/`, `Hosting/`, `Metrics/`, `OpenApi/`) and compiled into a single `Tests_run` executable discovered by `gtest_discover_tests`.

Run after configuring (tests are ON by default at top level):

```bash
ctest --test-dir build --output-on-failure
# or run a focused binary:
./build/test/Tests_run --gtest_filter=RouterSpec.*
```

CI invokes `ctest --test-dir ./test` from inside the `build/` dir — both forms work locally because CMake registers the tests at the top.

## Layout

- `src/Baldr/` — library sources. Public entrypoint is `src/Baldr/Baldr.hpp`; extension glue is `BaldrExtension.{hpp,cpp}`. Public surface includes `WebApplication`, `Router`, `HttpServer`, `HttpRequestParser`, middleware (`CorsMiddleware`, `RateLimitMiddleware`, `RequestIdMiddleware`, `ExceptionHandlerMiddleware`, `LoggingMiddleware`, `CsrfMiddleware`, `SecurityHeadersMiddleware`, `CompressionMiddleware`, `MetricsMiddleware`), `RateLimiter`, `WorkerPool`, `IResult` and `Results` (`TextResult`, `JsonResult`, `ContentResult`, `StatusResult`, `StreamingResult`, `FileStreamResult`), and OpenAPI (`OpenApiSpecService`, `SpecBuilder`, etc.).
- `examples/` — one subdir per app, each with its own `CMakeLists.txt`: `HelloWorld`, `HelloService`, `WeatherForecast`, `Devices`, `StaticFiles`, `FileStream`, `OpenApiExample`.
- `test/src/` — GoogleTest specs (`*Spec.cpp`).
- `benchmarks/wrk/` — wrk scripts; **not** wired into CMake (run manually).
- `docs/` — Zensical site source.
- `wiki/` — generated mirror of `docs/`, **synced by CI**, not edited by hand.
- `CHANGELOG.md` — keep releases in sync; bump `project(Baldr VERSION ...)` in `CMakeLists.txt:3`.

## Documentation

Site config is `zensical.toml`. Preview/build with Zensical (CI also installs `mkdocs-material`):

```bash
pip install zensical mkdocs-material
zensical serve      # http://localhost:8000
zensical build --clean   # output in site/ (gitignored)
```

Authoring rules (enforced style):

- Kebab-case filenames (`get-started.md`). One concept per page.
- Reference source/examples via GitHub permalinks.
- Long code fences must have a `title=` attribute and `linenums="1"`.
- Material admonitions (`!!! note`, `!!! tip`, `!!! warning`) and grid cards (`<div class="grid cards" markdown>`) for "next steps".

## CI

- `.github/workflows/cmake-multi-platform.yml` — build + ctest on Ubuntu (gcc-16). Triggered on push/PR to `main`, ignoring `*.md` and `docs/**`.
- `.github/workflows/docs.yml` — builds the Zensical site and deploys to GitHub Pages via `actions/deploy-pages` on pushes that touch `docs/**`, `zensical.toml`, or this workflow. Requires repo setting **Settings → Pages → Source = "GitHub Actions"**.
- `.github/workflows/sync-wiki.yml` — mirrors `docs/` into the GitHub Wiki on pushes to `main` that touch `docs/**`. Treat `wiki/` as CI output; do not hand-edit.

## Conventions

- New public API follows `src/Baldr/` patterns: smart pointers, `std::function`, RAII, `skr::ApplicationBuilder` DI. Handlers may return any `IResult` subclass or types serialised by results.
- Middleware contract: `IMiddleware::Handle` takes a mutable `HttpRequest&` so middlewares can attach context.
- TLS is unsupported on `HttpServer` by design — don't add TLS options without discussion.
- **Doxygen documentation is required on every public API.** All public types, functions, methods and free functions exposed in headers under `src/Baldr/` (and any new public surface added elsewhere) MUST carry Doxygen-style block comments. Use `@brief`, `@param`, `@tparam`, `@return`, `@throws`, `@note` and `@code`/`@endcode` as appropriate. The single existing exception is the "no general comments" rule below — Doxygen blocks are the explicit exception.
- Don't add comments to code unless explicitly asked. The sole exception is the Doxygen documentation requirement above.

# AGENTS.md

Guidance for Kilo (and other AI coding agents) working in this repository.

## Toolchain (C++)

- CMake **3.28+**, **C++23** (`CMAKE_CXX_STANDARD 26` in `CMakeLists.txt:8`).
- CI installs **gcc-14** on Linux (`gcc-13` and older may not compile the codebase — newer is safer). Clang and MSVC (`cl`) are also built in CI.
- Formatting: `.clang-format` (Microsoft base, 80-col, 4-space indent, no tabs, pointers left-aligned). Match it on new code.
- External deps are fetched via `FetchContent`: `trantor` (pinned `v1.5.28`, c-ares off, TLS off) and `skirnir` (`v0.22.0`). Do not add system-package fallbacks unless asked.

## Build

`CMakeLists.txt` makes examples **and** tests **default ON** when Baldr is the top-level project (see the `CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR` guard at `CMakeLists.txt:11`). The options `BALDR_BUILD_EXAMPLES` / `BALDR_BUILD_TESTS` default to `OFF` for consumers.

From the repo root:

```bash
cmake -S . -B build
cmake --build build --config Release
```

To skip examples:

```bash
cmake -S . -B build -DBALDR_BUILD_EXAMPLES=OFF
cmake --build build
```

The library is `baldr` (alias `baldr::baldr`), public include root is `src/` so consumers write `#include <Baldr/Baldr.hpp>`.

## Tests

GoogleTest is fetched in `test/CMakeLists.txt` (pinned to `main`, branch tag). All tests live under `test/src/*Spec.cpp` and are compiled into a single `Tests_run` executable discovered by `gtest_discover_tests`.

Run after configuring with `BALDR_BUILD_TESTS=ON` (the default at top level):

```bash
ctest --test-dir build --output-on-failure
```

Note: CI runs `ctest --test-dir ./test` (the per-subdir test file). Locally the project-level `ctest --test-dir build` works because CMake registers the tests at the top.

## Layout

- `src/Baldr/` — library sources. Public entrypoint header is `src/Baldr/Baldr.hpp`; extension glue is `BaldrExtension.{hpp,cpp}`. Headers include `WebApplication`, `Router`, `HttpServer`, `HttpRequestParser`, middleware (`CorsMiddleware`, `RateLimitMiddleware`, `RequestIdMiddleware`, `ExceptionHandlerMiddleware`, `LoggingMiddleware`), `RateLimiter`, `WorkerPool`, `IResult`/`Results`, etc.
- `examples/` — `HelloWorld`, `HelloService`, `WeatherForecast`, `Devices`. Each is its own subdir with a `CMakeLists.txt`.
- `test/src/` — GoogleTest specs (`*Spec.cpp`).
- `benchmarks/wrk/` — wrk scripts; not wired into CMake.
- `docs/` — Zensical site source.
- `wiki/` — generated mirror of `docs/`, **synced by CI**, not edited by hand.

## Documentation

Site config is `zensical.toml`. Preview/build with Zensical (CI also installs `mkdocs-material`):

```bash
pip install zensical mkdocs-material
zensical serve      # http://localhost:8000
zensical build --clean   # output in site/ (gitignored)
```

Authoring rules:

- Kebab-case filenames (`get-started.md`).
- One concept per page.
- Reference source/examples via GitHub permalinks.
- Long code fences must have a `title=` attribute and `linenums="1"`.
- Material admonitions (`!!! note`, `!!! tip`, `!!! warning`) and grid cards (`<div class="grid cards" markdown>`) for "next steps".

## CI

- `.github/workflows/cmake-multi-platform.yml` — build + ctest on Ubuntu (gcc-14, clang) and Windows (MSVC). Triggered on push/PR to `main`, ignoring `*.md` and `docs/**`.
- `.github/workflows/docs.yml` — builds the Zensical site and deploys to GitHub Pages via `actions/deploy-pages` on pushes that touch `docs/**`, `zensical.toml`, or this workflow. Requires repo setting **Settings → Pages → Source = "GitHub Actions"**.
- `.github/workflows/sync-wiki.yml` — mirrors `docs/` into the GitHub Wiki (`wiki/`) on pushes to `main` that touch `docs/**`. Treat `wiki/` as CI output; do not hand-edit.

## Conventions

- New public API should follow the patterns in `src/Baldr/` (smart pointers, `std::function`, RAII, `skr::ApplicationBuilder` DI). Handlers may return any `IResult` subclass (`TextResult`, `JsonResult`, `ContentResult`, `StatusResult`) or types serialised by results.
- Middleware contract: `IMiddleware::Handle` takes a mutable `HttpRequest&` so middlewares can attach context.
- TLS is unsupported on `HttpServer` by design — don't add TLS options without discussion.
- Don't add comments to code unless explicitly asked.
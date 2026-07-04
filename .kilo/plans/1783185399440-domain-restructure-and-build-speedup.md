# Baldr: Domain-Based Source Layout + Compile-Time Optimization

## Goal
Reorganise `src/Baldr/<flat>/` into a per-domain layout (`Http/`, `Middleware/`, `Metrics/`, `Application/`, `Hosting/`, plus the existing `OpenApi/`) and apply targeted pimpl + unity + PCH so a full rebuild of the library drops from "many seconds" to a few seconds and incremental rebuilds stop rebuilding the world.

## Decisions (resolved)
- **Hard break, major bump**: move files and update all `#include`s in one go; bump `project(... VERSION ...)` (e.g. `0.16.0` — confirm SemVer policy with maintainer). No compat shims.
- **Single target + unity + PCH**: keep `baldr` as one library this PR. Switch `GLOB_RECURSE` to explicit source lists.
- **Selective pimpl**: `Router`, `HttpServer`, `WebApplication`, `MetricsRegistry`. Class-internal templates (`MapRoute`) stay in headers but body lives in `.cpp` after move.
- **Tests mirror layout**: `test/src/Http/ParserSpec.cpp`, `test/src/Middleware/CompressionSpec.cpp`, etc.

## Final Layout
```
src/Baldr/
├── Baldr.hpp                          # umbrella (rewritten)
├── BaldrExtension.{hpp,cpp}           # stays at root
├── Http/
│   ├── Method.hpp
│   ├── StatusCode.hpp
│   ├── Request.hpp                    # from HttpRequest.hpp
│   ├── Response.hpp                   # from HttpResponse.hpp
│   ├── RequestParser.{hpp,cpp}
│   ├── Server.{hpp,cpp}               # HttpServer; merges ServerOptions.hpp
│   ├── Connection.{hpp,cpp}           # HttpConnection
│   ├── Router.{hpp,cpp}               # + RouteOptions.hpp, RouteRegistration.hpp
│   └── Results/
│       ├── Result.hpp
│       ├── JsonBody.hpp               # from JsonBody.hpp (keeps name)
│       ├── StreamingResult.hpp
│       └── FileStreamResult.hpp
├── Middleware/
│   ├── IMiddleware.hpp
│   ├── MiddlewareProvider.hpp
│   ├── Cors.hpp
│   ├── Csrf.hpp
│   ├── RateLimit/{Limiter.hpp, Middleware.hpp}
│   ├── Compression/{Middleware.{hpp,cpp}, Internal.{hpp,cpp}}
│   ├── SecurityHeaders.hpp
│   ├── RequestId.hpp
│   ├── Logging.hpp
│   └── ExceptionHandler.hpp
├── Metrics/
│   ├── Registry.{hpp,cpp}             # pimpl
│   └── Middleware.{hpp,cpp}
├── Application/
│   ├── WebApplication.{hpp,cpp}       # pimpl
│   ├── InFlightTracker.hpp
│   └── WorkerPool.hpp
├── Hosting/
│   ├── StringHelpers.hpp
│   └── MpMcPool.hpp                   # worker pool helper
└── OpenApi/                           # already correct; add MapOpenApi here too
    ├── BaldrOpenApiExtension.hpp
    ├── OpenApiOptions.hpp
    ├── OpenApiSpecService.{hpp,cpp}
    ├── JsonSchemaEmitter.{hpp,cpp}
    ├── MapOpenApi.hpp
    ├── RouteIntrospector.{hpp,cpp}
    └── SpecBuilder.{hpp,cpp}
```
Naming: short names inside each domain so `<Baldr/Http/Request.hpp>` is concise (drop the redundant `Http` prefix since the directory already says so). Exceptions: `JsonBody.hpp`, `Server.hpp`, `Router.hpp` keep their canonical names because they're widely referenced.

`CookieOptions.hpp`, `Tuple.hpp`, `HttpResult.hpp` fold into their natural home or are dropped (audit: `HttpResult.hpp` is 4 lines, likely redundant with `Result.hpp` — confirm during move).

## Task List (ordered for safe execution)

1. **Inventory & locking**
   - Audit every file under `src/Baldr/` and record its category in a scratch file (manual, inline during this task).
   - Audit `#include` graph: list every header that pulls in `<simdjson.h>`, `<trantor/...>`, `<regex>`, `<meta>`, `<map>`, `<unordered_map>` transitively. Output drives pimpl/forward-decl decisions in steps 5–7.

2. **Create new directories**
   - `mkdir -p src/Baldr/{Http/Results,Middleware/{RateLimit,Compression},Metrics,Application,Hosting}`.
   - No files moved yet.

3. **Move files (with `git mv`)** in this order to keep the working tree buildable between steps. After each batch, run `cmake --build build --target baldr -j` to surface breakage.
   - 3a. `Http/`: `HttpMethod.hpp`→`Method.hpp` (rename on move), `StatusCode.hpp`, `HttpRequest.hpp`→`Request.hpp`, `HttpResponse.hpp`→`Response.hpp`, `HttpRequestParser.{hpp,cpp}`, `HttpConnection.{hpp,cpp}`, `HttpServer.{hpp,cpp}`, `HttpServerOptions.hpp`→`Http/ServerOptions.hpp`, `Router.{hpp,cpp}`, `RouteOptions.hpp`, `RouteRegistration.hpp`, `StaticFilesInternal.hpp`→`Http/StaticFilesInternal.hpp`.
   - 3b. `Http/Results/`: `Result.hpp`, `JsonBody.hpp` (kept), `StreamingResult.hpp`, `FileStreamResult.hpp`, `HttpResult.hpp` (fold if redundant after audit).
   - 3c. `Middleware/`: `IMiddleware.hpp`, `MiddlewareProvider.hpp`, `CorsMiddleware.hpp`→`Cors.hpp`, `CsrfMiddleware.hpp`→`Csrf.hpp`, `RateLimitMiddleware.hpp`→`Middleware/RateLimit/Middleware.hpp`, `RateLimiter.hpp`→`Middleware/RateLimit/Limiter.hpp`, `SecurityHeadersMiddleware.hpp`→`SecurityHeaders.hpp`, `RequestIdMiddleware.hpp`→`RequestId.hpp`, `LoggingMiddleware.hpp`→`Logging.hpp`, `ExceptionHandlerMiddleware.hpp`→`ExceptionHandler.hpp`, `CompressionMiddleware.{hpp,cpp}`→`Middleware/Compression/Middleware.{hpp,cpp}`, `CompressionInternal.{hpp,cpp}`→`Middleware/Compression/Internal.{hpp,cpp}`.
   - 3d. `Metrics/`: `MetricsRegistry.{hpp,cpp}`→`Metrics/Registry.{hpp,cpp}`, `MetricsMiddleware.{hpp,cpp}`.
   - 3e. `Application/`: `WebApplication.{hpp,cpp}`, `InFlightTracker.hpp`, `WorkerPool.hpp`, `CookieOptions.hpp` (audit: if only `WebApplication` uses it, move with it; otherwise `Hosting/`).
   - 3f. `Hosting/`: `StringHelpers.hpp`, `MpMcPool.hpp`, `Tuple.hpp` (audit; drop if unused).
   - 3g. `OpenApi/`: move remaining `MapOpenApi.hpp` (already at root? — confirm; move into `OpenApi/`).

4. **Rewrite `#include` paths** (project-wide, single pass):
   - For each `.cpp` and `.hpp` under `src/`, `test/`, `examples/`, replace old paths with new `<Baldr/<Domain>/Foo.hpp>` form.
   - Use a script (sed in CMake configure step is not appropriate; instead a one-shot `find … -exec sed -i …`) with a deny-list of safe substitutions verified by the diff:
     - `"HttpMethod.hpp"` → `"Method.hpp"` (only when inside `Http/...` after the move; same for other renames).
     - `"HttpRequest.hpp"` → `"Request.hpp"` (Http-scoped).
     - `"HttpResponse.hpp"` → `"Response.hpp"`.
     - `"HttpServerOptions.hpp"` → `"ServerOptions.hpp"`.
     - `"CompressionInternal.hpp"` → `"Compression/Internal.hpp"` (only when included by middleware/Compression).
     - `"CompressionMiddleware.hpp"` → `"Compression/Middleware.hpp"`.
     - `"RateLimiter.hpp"` → `"RateLimit/Limiter.hpp"`, `"RateLimitMiddleware.hpp"` → `"RateLimit/Middleware.hpp"`.
     - All other paths: prefix `Http/`, `Middleware/`, `Metrics/`, `Application/`, `Hosting/` based on the file's new home.
   - Validate by full build + ctest.

5. **Rewrite the umbrella `Baldr.hpp` and `BaldrExtension.hpp`**
   - `Baldr.hpp` becomes:
     ```cpp
     #pragma once
     #include "BaldrExtension.hpp"
     #include "Http/Server.hpp"
     #include "Http/Router.hpp"
     #include "Http/Request.hpp"
     #include "Http/Response.hpp"
     #include "Application/WebApplication.hpp"
     #include "Metrics/Registry.hpp"
     #include "OpenApi/BaldrOpenApiExtension.hpp"
     ```
   - `BaldrExtension.hpp` includes only `<Baldr/Application/WebApplication.hpp>`.

6. **Pimpl pass**
   - `Router`: move `TrieNode`, `std::shared_mutex`, `std::map<HttpMethod, ...>`, `SchemaRegistry` member behind `struct Router::Impl; std::unique_ptr<Impl> pImpl;`. Public template `MapRoute` stays in header (templates can't move to .cpp easily without explicit instantiations). The internal `insert` overload, `Snapshot()`, `match*` move to .cpp unchanged.
   - `HttpServer`: move all trantor/EventLoop members behind pimpl. Header keeps only the public ctor + `Run()` + `Options`.
   - `WebApplication`: move `mRouter`, `mMiddlewareProvider` (and `RouteBuilder` PIMPL container) behind pimpl. Keep `MapGet/MapPost/...` and `Use<>` inline since they delegate to pimpl.
   - `MetricsRegistry`: already mostly pimpl-ready (`MetricsRegistry.{hpp,cpp}` exists); confirm registry storage is private and move any std::unordered_map<string, …> behind pimpl.
   - Add corresponding `.cpp` `pImpl->…` rewrites. Verify `Rule of Five` still holds (most of these types are move-only or non-copyable already; pimpl usually preserves that).

7. **Forward-decl pass in public headers**
   - Replace `#include "OpenApi/JsonSchemaEmitter.hpp"` in `RouteRegistration.hpp` with `namespace Baldr::OpenApi { class … }` forward decl (move `#include` back into the .cpp file if/when one exists; otherwise move the relevant logic out of the inline header).
   - In `WebApplication.hpp`, remove `#include`s pulled in only to support the pimpl private types — move them into the .cpp.
   - In `Router.hpp`, replace `#include <regex>`, `<map>`, `<unordered_map>`, `<shared_mutex>` with forward decls + pimpl. Keep `<functional>`, `<memory>`, `<string>` (Templates `LambdaTraits` use them; keep only the absolutely required STLs).
   - In `JsonBody.hpp`, keep `<simdjson.h>` ONLY if we move the templates to a `.cpp` (not feasible — they're templates). Acceptable: the file is already on the hot path of consumers; consider leaving the include. Document in code-review note: this is a known heavy include consumers will incur.

8. **CMake changes**
   - Replace `file(GLOB_RECURSE BALDR_SOURCES src/*.cpp)` with explicit lists grouped by domain (one block per directory, commented with `# --- <Domain> ---`). Keeps GLOB's drag-and-drop convenience while gaining correctness.
   - Add:
     ```cmake
     target_precompile_headers(baldr PRIVATE
         <string> <memory> <functional> <unordered_map> <vector> <utility>
         <Baldr/Baldr.hpp>
     )
     set_target_properties(baldr PROPERTIES
         UNITY_BUILD ON
         UNITY_BUILD_BATCH_SIZE 8
     )
     ```
   - Bump version: `project(Baldr VERSION 0.16.0 LANGUAGES C CXX)` (confirm maintainer's preference).
   - Add `find_package(Threads REQUIRED)` only if not already pulled in transitively (audit `trantor` linkage).

9. **Tests reorganization**
   - Mirror under `test/src/Http/`, `test/src/Middleware/Compression/`, `test/src/Middleware/RateLimit/`, `test/src/Metrics/`, `test/src/Application/`, `test/src/OpenApi/`.
   - Files: `HttpRequestParserSpec.cpp`→`Http/ParserSpec.cpp`, `HttpServerOptionsSpec.cpp`→`Http/ServerOptionsSpec.cpp`, `HttpRequestResponseSpec.cpp`→`Http/RequestResponseSpec.cpp`, `HttpConnectionMiddlewareChainSpec.cpp`→`Application/ConnectionMiddlewareChainSpec.cpp` (cross-domain), `RouterSpec.cpp`+`RouterMethodFallbackSpec.cpp`+`RouteOptionsSpec.cpp`→`Http/Router/` (single subfolder), `CompressionSpec.cpp`→`Middleware/Compression/CompressionSpec.cpp`, `CsrfMiddlewareSpec.cpp`→`Middleware/CsrfSpec.cpp`, `SecurityHeadersMiddlewareSpec.cpp`→`Middleware/SecurityHeadersSpec.cpp`, `RateLimiterSpec.cpp`→`Middleware/RateLimit/LimiterSpec.cpp`, `MiddlewareSpec.cpp`→`Middleware/IMiddlewareSpec.cpp`, `InFlightTrackerSpec.cpp`→`Application/InFlightTrackerSpec.cpp`, `MetricsRegistrySpec.cpp`→`Metrics/RegistrySpec.cpp`, `JsonSchemaEmitterSpec.cpp`+`MapStaticFilesSpec.cpp`+`OpenApiSpec.cpp`+`JsonBodySpec.cpp`+`ResultSpec.cpp`+`StreamingResultSpec.cpp`+`FileStreamResultSpec.cpp`+`StringHelpersSpec.cpp` → respective mirrors.
   - `test/CMakeLists.txt`: switch to `file(GLOB_RECURSE test/src/*Spec.cpp)` (already used? verify; replace with explicit list if needed). Each spec file's `#include` paths get the same rewrite as in step 4.

10. **Examples & docs**
    - `examples/**/*.cpp`: rewrite `#include` paths in step 4 capture; verify `cmake --build build` for each example subdir.
    - `docs/`: README page ("Project Structure" if present) and any code samples citing include paths must be updated to match.

11. **CI adjustments**
    - The CI workflow builds with `cmake --build build --config Release`. No CMake contract changes, so should pass unchanged once the version bump updates `CMakeLists.txt`.
    - Add `iwyu_tool` job to `.github/workflows/cmake-multi-platform.yml` (optional, separate gate).

12. **Validation checklist**
    - `cmake -S . -B build` clean configure.
    - `cmake --build build --config Release -j` clean rebuild; record wall time.
    - `ctest --test-dir build --output-on-failure` — all tests pass; record delta.
    - Touch `src/Baldr/Http/Server.cpp` only → rebuild — should recompile only that TU + relink `baldr` + relink tests/examples that include it (proof that headers aren't dragging in unrelated includes).
    - `ninja -t deps` (or `cmake --build build --target help`) — confirm the dependency graph is what we expect.
    - `clang-tidy` (optional): pre-existing config in repo? If yes, run. If no, defer.
    - Bump-major annotation in CHANGELOG describing the new include paths and removed compat.

## Risks & Mitigations
- **`MapRoute`/`BindRoute` are header templates** → can't be pimpl'd; the heavy type they need (`Router` definition) is the cost we accept. The huge payoff comes from pimpling `Router`'s member variables and moving `Result.hpp`/`simdjson` out of `WebApplication.hpp` and `Router.hpp`.
- **`JsonBody.hpp` includes `<simdjson.h>` and `<meta>` publicly** → documented as a known cost. If profiling shows it matters, introduce a `.tpp`/explicit-instantiation approach in a follow-up.
- **`file(GLOB)` hides new files from CMake** → after step 8, we use explicit lists; new files require a CMake edit (this is intentional: forces the author to think about pimpl/STL implications).
- **Hard break for downstream consumers** (none on the public registry yet, but examples and any `*/find_package(baldr)` users): mitigated by the major version bump and CHANGELOG entry. Consider publishing a one-page MIGRATING.md.

## Out of Scope (defer)
- Per-domain static-library split (deferred per decision; revisit when package size or ABI granularity complaints arise).
- IWYU tooling in CI (defer to allow the restructure PR to stay small).
- Removing `HttpResult.hpp` (pending audit; if confirmed redundant, remove in same PR; otherwise leave).

## Open Questions
- Confirm `Baldr` version policy: is `0.16.0` correct, or do we go `1.0.0` for the breaking restructure? **Need maintainer answer.**
- Confirm `HttpResult.hpp` is redundant with `Result.hpp` (vs. additive) before deletion.

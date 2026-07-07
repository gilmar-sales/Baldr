# Changelog

All notable changes to Baldr are documented here. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Router: `**` greedy catch-all segment. Registering `prefix/**` matches `prefix`, `prefix/`, and any nested path under `prefix`; the remainder is captured into `params["filepath"]` (without the leading `/`).
- `MapStaticFilesSpec` covering nested serving, directory index fallback, and rejection of raw / percent-encoded / backslash / NUL traversal vectors.
- `IResult` interface with `TextResult`, `JsonResult`, `ContentResult`, `StatusResult` and `Results` factory functions in `src/Baldr/Result.hpp`. Handlers may return any `IResult` subclass by value.
- `WebApplication::MapGroup(prefix, setup)` for nested route prefixes via `RouteBuilder`.
- `WebApplication::MapStaticFiles(urlPrefix, rootPath)` with mime detection, path-traversal guard, and `weakly_canonical` root confinement.
- `WebApplication::MapPut`, `MapDelete`, `MapPatch` overloads.
- `HttpRequest::cookies` populated by the parser from the `Cookie` header.
- `CorsMiddleware` (configurable origin, methods, headers, credentials, max-age; short-circuits `OPTIONS`).
- `RequestIdMiddleware` (echoes `X-Request-ID` or generates a 16-char hex id).
- `ExceptionHandlerMiddleware` (catches `std::exception` and unknown exceptions, emits 500).
- `WorkerPool` service registered as a singleton in `BaldrExtension` for offloading blocking work to a thread pool.
- `baldr::MapScalarUi(*app)` helper mounting an embedded Scalar API reference UI. The vendored Scalar 1.62.4 bundle (JS, CSS, HTML wrapper) is pulled into the translation unit via `std::embed` (C++26, P1967R14; requires gcc-15+) — no runtime files, no CDN. The helper registers `skr::Logger<ScalarUi>` through `BaldrExtension` and emits a ctrl+clickable `http://0.0.0.0:{port}{mount}` URL via `LogInformation` so terminals can open the UI directly from the log line.
- Cookie parsing tests, content-result test, middleware tests, route-prefix test, real `RateLimiter` tests (capacity, refill, thread safety, LRU eviction).
- Streamed-buffer integration tests (byte-by-byte feed, pipelining, arbitrary 3-byte chunking).
- `ScalarUiSpec` covering embedded asset byte counts, placeholders, and `AsStringView` size invariants.
- Handlers may return `std::variant<...>` to model responses that take one of several shapes (e.g. product-or-NotFound). The active alternative is dispatched through the same rules as a non-variant return (`IResult::Apply`, JSON serialization, string coercion, etc.). `IStreamingResult` alternatives inside a variant are rejected at compile time. The framework now auto-derives one `responses` entry per status code from the variant alternatives, including non-`TypedResult` `IResult` subclasses (previously skipped). `JsonResult<T, Status>` became a template; see "Changed" below.
- `RouteRegistration::WithResponseContentType(std::string mime)` to pin the OpenAPI media type for status `200` when paired with `WithResponseSchemaJson(...)` or `WithResponseType<T>()` (defaults to `application/json`).
- `IResult` virtuals `StatusFor()`, `ContentTypeFor()`, `SchemaJsonFor()` (with a sensible default) so the OpenAPI generator can render a faithful schema and media type for any `IResult` subclass, including the legacy `TextResult`/`StatusResult`/`ContentResult`. `TextResult` and `ContentResult` are now default-constructible so the generator can introspect them without an instance.
- `IHealthCheck` interface (`src/Baldr/Application/IHealthCheck.hpp`) and a DI-driven `WebApplication::MapHealthChecks(paths, livePath)` overload that resolves every registered `IHealthCheck` via `ServiceProvider::GetServices<IHealthCheck>()` at registration time.
- `HealthStatus` enum (`Healthy`, `Degraded`, `Unhealthy`) and `HealthCheckResult` struct in `src/Baldr/Application/HealthCheckResult.hpp`, with `HealthCheckResult::{Healthy, Degraded, Unhealthy}` factories. `HealthCheckResult::data` accepts a pre-serialized JSON fragment that is inlined verbatim into the response body so checks can surface structured detail (latency, queue depth, replica set, ...).

### Changed
- `WebApplication::MapStaticFiles` rewritten to support arbitrary depth (`urlPrefix/**`), fall back to `index.html` on directory requests, and reject path-traversal attempts (raw `..`, percent-encoded `..`, backslashes, NUL) before any filesystem call. The canonical-target prefix check now requires the next byte to be a path separator, blocking sibling directories whose names share the root prefix.
- **Breaking**: `WebApplication::MapHealthChecks(paths, checks, livePath)` removed. The only supported signature is now `MapHealthChecks(paths, livePath = {})`, which resolves checks from the DI container. Migrate any `HealthCheckRegistration` predicates by implementing `IHealthCheck` and registering them with `AddTransient<IHealthCheck, MyCheck>()`.
- **Breaking**: `IHealthCheck::Check` now returns `HealthCheckResult` instead of `bool`. The aggregator renders `status`/`description`/`error`/`data` per check; only `Unhealthy` flips the endpoint to `503` (`Degraded` still yields `200`). Exceptions thrown by `Check` are caught and converted to `HealthCheckResult::Unhealthy` with `what()` as the `error`.
- `HttpRequestParser` rewritten as a single-pass parser with explicit byte offsets; rejects header folding, extra whitespace, duplicate `Content-Length`; enforces size caps for headers, header values, paths, and bodies.
- `Router` migrated from raw `new`/`TrieNode*` to `std::unique_ptr<TrieNode>` (recursive); guarded by `std::shared_mutex` for concurrent reads.
- `MiddlewareProvider` gained `Seal()` — `HttpServer::Run` snapshots the factory list before serving, eliminating per-request contention.
- `IMiddleware::Handle` takes `HttpRequest&` (mutable) so middlewares can attach context (e.g. request IDs, auth claims).
- `RateLimiter` rewritten to use `try_emplace` under a single lock (fixes TOCTOU), keeps a templated constructor for any `std::chrono::duration`, and applies LRU eviction at `maxTrackedClients` (default 10 000).
- `HttpConnection` caps the per-connection accumulator at 10 MB and force-closes on overflow.
- `HttpServer` installs `SIGINT`/`SIGTERM` handlers that call `Stop()`; documented that TLS is unsupported.
- `SpecBuilder` now renders the real media type for each response (e.g. `text/plain` for `TextResult`/`NotFoundResult`, `application/octet-stream` when paired with `WithResponseContentType`) instead of hard-coding `application/json`. A new `responseContentTypesJson` metadata key carries the per-status media type.
- `JsonResult` is now a template `JsonResult<T, Status>` that keeps the payload as a structured `T` and serialises via `simdjson::to_json_string` in `Apply`. The OpenAPI generator emits a `$ref` to the registered schema under `Status` instead of a generic `{"type":"string"}` placeholder. `Results::Json` is also a template: call `Results::Json<UserDto, baldr::StatusCode::OK>(value)`.

### Fixed
- `WebApplication` no longer emits the bogus `plain/text` content type; `const char*` returns are handled explicitly; non-serializable returns fall through to a 500 response instead of a silent empty body.
- `RateLimiter` race that could double-consume tokens or crash on a fresh `clientId`.

### Added
- `baldr::parseJson<T>` (`FromBody<T>`, `baldr::HttpRequest`) now deserialises beyond the five primitive members — `std::optional<U>`, `std::array<U, N>`, `std::vector<U>`, and nested reflectable structs (`AddressDto { city; street; }`, etc.) are supported recursively, with the same `IsReflectableStruct` / `Detail::IsSupportedField` trait widening applied to `EmitStructSchema<T>()` so the OpenAPI emitter stays aligned. Missing optional/vector/array/struct fields default to `nullopt` / empty / zero-initialised rather than failing the request. Error paths are now dotted/array-indexed (e.g. `address.city`, `tags[2]`, `billing.city`) and surface as `JsonBodyResult::Error::field`.

### Notes
- The `WebApplication::MapRoute` template was promoted to a public member so the new `RouteBuilder` can register routes through it. Behaviour is unchanged for existing call sites.

## [0.16.0] - 2026-07-04

### Changed (BREAKING)
- **Per-domain include layout.** All public headers were relocated from the flat `src/Baldr/` tree into per-domain subdirectories matching OpenApi conventions. Consumers must update their `#include` paths:
    - `#include <Baldr/HttpRequest.hpp>` → `#include <Baldr/Http/Request.hpp>`
    - `#include <Baldr/HttpResponse.hpp>` → `#include <Baldr/Http/Response.hpp>`
    - `#include <Baldr/HttpServer.hpp>` → `#include <Baldr/Http/Server.hpp>`
    - `#include <Baldr/HttpConnection.hpp>` → `#include <Baldr/Http/Connection.hpp>`
    - `#include <Baldr/HttpRequestParser.hpp>` → `#include <Baldr/Http/RequestParser.hpp>`
    - `#include <Baldr/HttpServerOptions.hpp>` → `#include <Baldr/Http/ServerOptions.hpp>`
    - `#include <Baldr/HttpMethod.hpp>` → `#include <Baldr/Http/Method.hpp>`
    - `#include <Baldr/HttpResult.hpp>` → `#include <Baldr/Http/Results/HttpResult.hpp>`
    - `#include <Baldr/Router.hpp>` → `#include <Baldr/Http/Router.hpp>`
    - `#include <Baldr/RouteOptions.hpp>` → `#include <Baldr/Http/RouteOptions.hpp>`
    - `#include <Baldr/RouteRegistration.hpp>` → `#include <Baldr/Http/RouteRegistration.hpp>`
    - `#include <Baldr/Result.hpp>` → `#include <Baldr/Http/Results/Result.hpp>`
    - `#include <Baldr/JsonBody.hpp>` → `#include <Baldr/Http/Results/JsonBody.hpp>`
    - `#include <Baldr/StreamingResult.hpp>` → `#include <Baldr/Http/Results/StreamingResult.hpp>`
    - `#include <Baldr/FileStreamResult.hpp>` → `#include <Baldr/Http/Results/FileStreamResult.hpp>`
    - `#include <Baldr/WebApplication.hpp>` → `#include <Baldr/Application/WebApplication.hpp>`
    - `#include <Baldr/InFlightTracker.hpp>` → `#include <Baldr/Application/InFlightTracker.hpp>`
    - `#include <Baldr/WorkerPool.hpp>` → `#include <Baldr/Application/WorkerPool.hpp>`
    - `#include <Baldr/MetricsRegistry.hpp>` → `#include <Baldr/Metrics/Registry.hpp>`
    - `#include <Baldr/MetricsMiddleware.hpp>` → `#include <Baldr/Metrics/Middleware.hpp>`
    - `#include <Baldr/IMiddleware.hpp>` → `#include <Baldr/Middleware/IMiddleware.hpp>`
    - `#include <Baldr/MiddlewareProvider.hpp>` → `#include <Baldr/Middleware/MiddlewareProvider.hpp>`
    - `#include <Baldr/CorsMiddleware.hpp>` → `#include <Baldr/Middleware/Cors.hpp>`
    - `#include <Baldr/CsrfMiddleware.hpp>` → `#include <Baldr/Middleware/Csrf.hpp>`
    - `#include <Baldr/ExceptionHandlerMiddleware.hpp>` → `#include <Baldr/Middleware/ExceptionHandler.hpp>`
    - `#include <Baldr/LoggingMiddleware.hpp>` → `#include <Baldr/Middleware/Logging.hpp>`
    - `#include <Baldr/RequestIdMiddleware.hpp>` → `#include <Baldr/Middleware/RequestId.hpp>`
    - `#include <Baldr/SecurityHeadersMiddleware.hpp>` → `#include <Baldr/Middleware/SecurityHeaders.hpp>`
    - `#include <Baldr/RateLimiter.hpp>` → `#include <Baldr/Middleware/RateLimit/Limiter.hpp>`
    - `#include <Baldr/RateLimitMiddleware.hpp>` → `#include <Baldr/Middleware/RateLimit/Middleware.hpp>`
    - `#include <Baldr/CompressionMiddleware.hpp>` → `#include <Baldr/Middleware/Compression/Middleware.hpp>`
    - `#include <Baldr/CompressionInternal.hpp>` → `#include <Baldr/Middleware/Compression/Internal.hpp>` (still internal)
    - `#include <Baldr/StringHelpers.hpp>` → `#include <Baldr/Hosting/StringHelpers.hpp>`
- Bumped CMake `project(... VERSION ...)` from 0.15.1 to 0.16.0 to mark this as a breaking release.

### Added
- Pimpl (private `Impl`) on `Router`, `HttpServer`, `WebApplication`, `MetricsRegistry`. Public headers stop pulling in `<regex>`, `<trantor/...>`, `<atomic>`, `<unordered_map>`, and the trie types — touching internal members no longer forces a recompile of every consumer.
- Precompiled header (`std::string`, `std::memory`, `std::functional`, `std::unordered_map`, `std::vector`, `std::utility`, `<Baldr/Baldr.hpp>`) on the `baldr` target.
- CMake unity builds (`UNITY_BUILD ON`, `UNITY_BUILD_BATCH_SIZE 4`) on the `baldr` target to amortise include-graph cost across small groups of `.cpp` files.

### Internal
- Source list switched from `file(GLOB_RECURSE)` to an explicit list grouped by domain in `CMakeLists.txt` (new files now require a CMake edit, which intentionally surfaces the cost of touching public headers).
- Test layout mirrors the new domain layout (`test/src/Http/`, `test/src/Middleware/`, `test/src/Metrics/`, `test/src/Application/`, `test/src/Hosting/`, `test/src/OpenApi/`).
- New umbrella header `Baldr.hpp` re-exports the most-used public headers; `BaldrExtension.hpp` was lightened to only pull `<Baldr/Application/WebApplication.hpp>`.

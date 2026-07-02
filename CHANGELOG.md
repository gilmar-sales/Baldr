# Changelog

All notable changes to Baldr are documented here. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `IResult` interface with `TextResult`, `JsonResult`, `ContentResult`, `StatusResult` and `Results` factory functions in `src/Baldr/Result.hpp`. Handlers may return any `IResult` subclass by value.
- `WebApplication::MapGroup(prefix, setup)` for nested route prefixes via `RouteBuilder`.
- `WebApplication::MapStaticFiles(urlPrefix, rootPath)` with mime detection, path-traversal guard, and `weakly_canonical` root confinement.
- `WebApplication::MapPut`, `MapDelete`, `MapPatch` overloads.
- `HttpRequest::cookies` populated by the parser from the `Cookie` header.
- `CorsMiddleware` (configurable origin, methods, headers, credentials, max-age; short-circuits `OPTIONS`).
- `RequestIdMiddleware` (echoes `X-Request-ID` or generates a 16-char hex id).
- `ExceptionHandlerMiddleware` (catches `std::exception` and unknown exceptions, emits 500).
- `WorkerPool` service registered as a singleton in `BaldrExtension` for offloading blocking work to a thread pool.
- Cookie parsing tests, content-result test, middleware tests, route-prefix test, real `RateLimiter` tests (capacity, refill, thread safety, LRU eviction).
- Streamed-buffer integration tests (byte-by-byte feed, pipelining, arbitrary 3-byte chunking).

### Changed
- `HttpRequestParser` rewritten as a single-pass parser with explicit byte offsets; rejects header folding, extra whitespace, duplicate `Content-Length`; enforces size caps for headers, header values, paths, and bodies.
- `Router` migrated from raw `new`/`TrieNode*` to `std::unique_ptr<TrieNode>` (recursive); guarded by `std::shared_mutex` for concurrent reads.
- `MiddlewareProvider` gained `Seal()` — `HttpServer::Run` snapshots the factory list before serving, eliminating per-request contention.
- `IMiddleware::Handle` takes `HttpRequest&` (mutable) so middlewares can attach context (e.g. request IDs, auth claims).
- `RateLimiter` rewritten to use `try_emplace` under a single lock (fixes TOCTOU), keeps a templated constructor for any `std::chrono::duration`, and applies LRU eviction at `maxTrackedClients` (default 10 000).
- `HttpConnection` caps the per-connection accumulator at 10 MB and force-closes on overflow.
- `HttpServer` installs `SIGINT`/`SIGTERM` handlers that call `Stop()`; documented that TLS is unsupported.

### Fixed
- `WebApplication` no longer emits the bogus `plain/text` content type; `const char*` returns are handled explicitly; non-serializable returns fall through to a 500 response instead of a silent empty body.
- `RateLimiter` race that could double-consume tokens or crash on a fresh `clientId`.

### Notes
- The `WebApplication::MapRoute` template was promoted to a public member so the new `RouteBuilder` can register routes through it. Behaviour is unchanged for existing call sites.

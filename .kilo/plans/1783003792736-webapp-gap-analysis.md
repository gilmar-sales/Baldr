# Baldr — Gap Analysis & Recommendations (Plan)

## Context

Baldr is a small C++23 web microframework built on trantor (networking) and skirnir (DI/logging). It already provides:

- HTTP/1.1 parsing (`HttpRequestParser`) with strict single-pass validation, duplicate-`Content-Length` rejection, and per-connection accumulator caps.
- Trie-based router with route params (`/users/{id}`), wildcard catch-all (`prefix/**`), route groups (`MapGroup`), and per-method trie.
- Middleware pipeline (`IMiddleware`) with sealing at startup to remove per-request contention. Built-ins: `LoggingMiddleware`, `RequestIdMiddleware`, `CorsMiddleware`, `RateLimitMiddleware`, `ExceptionHandlerMiddleware`.
- Static file serving (`MapStaticFiles`) with mime detection, index fallback, and path-traversal protection (raw, percent-encoded, backslash, NUL).
- Results (`IResult` / `Results` factory) so handlers can return `TextResult`, `JsonResult`, `ContentResult`, `StatusResult`.
- Cookie parsing on requests and cookie emission on responses with `SameSite` / `HttpOnly` / `Secure` / `Max-Age` / `Domain`.
- DI via skirnir (`WebApplication` injected with `Router`, `MiddlewareProvider`).
- `HttpServer` with SIGINT/SIGTERM graceful shutdown. TLS explicitly **not** supported (documented design choice).
- GoogleTest suite covering parser, router, middleware chain, rate limiter, results, static files, string helpers.

The gaps below are what is **missing or shallow** when measured against the features/quality bar expected of a "good" modern web framework. Each gap lists the observation, the recommendation, and a suggested first deliverable. They are grouped by area and ordered roughly by impact/priority within each group.

---

## 1. Transport & protocol

### Gap 1.1 — No HTTP keep-alive control / per-connection request limits
- **Observation**: `HttpConnection` parses pipelined bytes via `kMaxAccumulatorBytes`, but I did not find explicit handling of HTTP/1.1 keep-alive vs. close, no max-requests-per-connection cap, and no `Connection: close` opt-in. Pipelining is a known DoS amplifier.
- **Recommendation**: Honor the `Connection` request header (and HTTP/1.0 default of `close`). Add a configurable `maxRequestsPerConnection` (default e.g. 1000) and force-close after the cap. Surface `keepAlive` on `HttpServerOptions`.
- **First deliverable**: Extend `HttpServerOptions` with `keepAlive` + `maxRequestsPerConnection`; in `HttpConnection::onMessage` track request count and close when threshold hit; add tests.

### Gap 1.2 — HTTP/2 and HTTP/3 not supported
- **Observation**: trantor speaks HTTP/1.1 only.
- **Recommendation**: Document explicitly that Baldr targets HTTP/1.1 and recommend an upstream ALPN-aware proxy (nginx, envoy, Cloudflare) for HTTP/2/3. Do **not** attempt to bolt H2 onto trantor without upstream discussion — note this is consistent with the existing "TLS not supported" stance.
- **First deliverable**: Add a paragraph to `docs/get-started.md` "Production deployment" section; reference reverse-proxy patterns.

### Gap 1.3 — TLS still unsupported
- **Observation**: Acknowledged in code comment and AGENTS.md as deliberate.
- **Recommendation**: Keep the stance, but make it user-facing: in `HttpServerOptions`, throw a clear error if a user attempts to bind with `https://` or sets a cert path. Document the reverse-proxy termination pattern.
- **First deliverable**: Update `docs/setup/build.md` (or new `docs/setup/production.md`) with a sample nginx TLS-terminating snippet.

---

## 2. Routing

### Gap 2.1 — No route constraints (regex/typeof/min/max) or route metadata
- **Observation**: Router supports only `{name}` params and `**` catch-all. ASP.NET/Express/Fastify support route constraints and per-route metadata (auth required, rate-limit policy, OpenAPI hints).
- **Recommendation**: Add a richer `MapHttpGet` (or builder) overload accepting constraints, e.g. `MapGet("/users/{id:int}", ...)` or a `RouteOptions` struct. Allow `RouteOptions::metadata` (key/value bag) so middleware can read per-route policy.
- **First deliverable**: Add `RouteOptions` struct with `metadata` map; extend router to attach it to `RouteEntry`; expose via `HttpRequest` (e.g. `request.route.metadata`) for middleware consumption.

### Gap 2.2 — No HEAD/auto-HEAD support for GET routes
- **Observation**: `Router` has an explicit `HttpMethod::Head` trie; nothing maps GET handlers to HEAD automatically. Same for OPTIONS — `CorsMiddleware` only adds headers globally.
- **Recommendation**: When a GET handler is registered and HEAD is not, dispatch HEAD by reusing the GET handler but suppressing the body. For OPTIONS, support per-route `Allow` listing or auto-derive from registered methods on the same path.
- **First deliverable**: Auto-HEAD for GET; add tests.

### Gap 2.3 — No 405 Method Not Allowed / Allow response when path matches another method
- **Observation**: When a path exists for one method and another method is requested, framework likely returns 404.
- **Recommendation**: In `Router::match`, on method miss, return a second-best match with method information so `HttpConnection` can emit 405 + `Allow:` header listing valid methods for the path.
- **First deliverable**: Return a struct `MatchResult { optional<RouteEntry> entry; std::vector<HttpMethod> allowedMethodsOnPath; }`; emit 405 when `entry` empty but `allowedMethodsOnPath` non-empty.

### Gap 2.4 — No route listing / debugging endpoint (debug builds)
- **Observation**: No way to inspect registered routes at runtime.
- **Recommendation**: Add `WebApplication::EnableRouteListing()` that registers a guarded `/__routes` debug endpoint (only in debug or behind a feature flag) returning JSON of method+path+metadata.
- **First deliverable**: Hidden behind `BALDR_DEBUG_ROUTES` CMake option; add docs.

---

## 3. Middleware & cross-cutting

### Gap 3.1 — No built-in authentication/authorization primitives
- **Observation**: No JWT, OAuth, API key, or session middleware. Frameworks ship at least one reference implementation (e.g. cookie auth in ASP.NET).
- **Recommendation**: Provide a thin **API-key middleware** as a first-class example (`ApiKeyMiddleware` with `ApiKeyOptions { header, keys, exemptPaths }`). Defer JWT/OAuth to third-party extensions but document how to plug them in. Also add `RequireAuthorization` helper that consults `request.user` set by auth middleware.
- **First deliverable**: `ApiKeyMiddleware` + tests + docs/extensions/auth.md.

### Gap 3.2 — No CSRF protection helper
- **Observation**: Cookies can be set with `SameSite`, but there's no anti-CSRF token primitive (double-submit cookie, etc.).
- **Recommendation**: Add `CsrfMiddleware` using double-submit cookie pattern: set a non-HttpOnly-readable CSRF cookie, require matching header on unsafe methods. Make it opt-in.
- **First deliverable**: `CsrfMiddleware` + tests; document SameSite interaction.

### Gap 3.3 — No security headers middleware
- **Observation**: No defaults for `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy`, `Strict-Transport-Security`, `Permissions-Policy`, `Content-Security-Policy`.
- **Recommendation**: Provide a `SecurityHeadersMiddleware` with sane defaults and per-header opt-out.
- **First deliverable**: `SecurityHeadersMiddleware` (HSTS conditional on TLS being terminated upstream, documented).

### Gap 3.4 — No request body size per-route or per-endpoint override
- **Observation**: `HttpRequestParser::maxBodySize` is global (100 MB).
- **Recommendation**: Allow `RouteOptions::maxBodyBytes`; middleware/router to enforce before handler dispatch.
- **First deliverable**: Wire per-route override; tests.

### Gap 3.5 — No streaming responses / chunked transfer
- **Observation**: `HttpResponse.body` is `std::string` only. Large file or SSE responses must be fully buffered.
- **Recommendation**: Introduce `IResult::WriteAsync(trantor::TcpConnection&)` (or a streaming variant) for chunked transfer; add `StreamResult` and `ServerSentEventsResult` examples. Keep current `Apply(HttpResponse&)` for back-compat.
- **First deliverable**: New `IResultStreaming` interface + `FileStreamResult` + tests.

### Gap 3.6 — Request body access API is raw `std::string`
- **Observation**: `HttpRequest::body` is a string. JSON/form/multipart parsing is the user's problem.
- **Recommendation**: Provide ergonomic parsing helpers as opt-in utilities (no middleware magic):
  - `JsonBody<T>()` helper that reads `request.body` via simdjson (already used for serialization).
  - `FormData` helper for `application/x-www-form-urlencoded`.
  - Note: multipart would benefit from a small dedicated library or extension.
- **First deliverable**: `JsonBody<T>()` template + example; add `docs/usage/requests.md`.

### Gap 3.7 — No global error/exception customization
- **Observation**: `ExceptionHandlerMiddleware` always returns plain-text 500 with the exception `what()` — leaks internals.
- **Recommendation**: Add `ExceptionHandlerOptions { std::function<HttpResponse(const std::exception&)> mapper; bool includeDetailsInDev = true; }`. Default mapper returns a generic 500 in release, details in debug.
- **First deliverable**: Configurable mapper + tests verifying that `what()` is **not** leaked by default.

---

## 4. Static files & content negotiation

### Gap 4.1 — No ETag / Last-Modified / conditional GET / Range support
- **Observation**: `MapStaticFiles` exists, but no caching or partial-content semantics.
- **Recommendation**: Compute `ETag` (e.g. size + mtime hash) and `Last-Modified` at file open. Handle `If-None-Match`/`If-Modified-Since` → 304, and `Range` → 206 with proper `Content-Range`. This is essential for any web framework serving assets.
- **First deliverable**: ETag + Last-Modified + 304; then Range/206.

### Gap 4.2 — No compression (gzip / brotli / zstd)
- **Observation**: Bodies are sent uncompressed.
- **Recommendation**: Add a `CompressionMiddleware` that negotiates `Accept-Encoding` and compresses text-like bodies (configurable mime whitelist, min size threshold). Use `zlib` for gzip, optional `brotli`/`zstd` later.
- **First deliverable**: `CompressionMiddleware` (gzip via zlib) + tests for negotiation and threshold.

### Gap 4.3 — No SPA fallback or directory listing toggle
- **Observation**: `MapStaticFiles` falls back to `index.html` on directories but has no SPA fallback for unmatched paths under the prefix, and no way to disable directory fallback in favor of 404.
- **Recommendation**: Add options struct `StaticFilesOptions { bool spaFallback = false; std::string spaFallbackFile = "index.html"; bool enableDirectoryListing = false; long cacheControlMaxAge = 0; }`.
- **First deliverable**: `StaticFilesOptions` wired through `MapStaticFiles`; tests for SPA fallback.

### Gap 4.4 — No MIME customization
- **Observation**: Hardcoded mime table.
- **Recommendation**: Allow `StaticFilesOptions::extraMimeTypes` map and override table.

---

## 5. Observability

### Gap 5.1 — No metrics (request count, latency histogram, error rate, in-flight)
- **Observation**: Logging only.
- **Recommendation**: Add a `MetricsMiddleware` (or service) that maintains counters/histograms in-memory and exposes them on a `/metrics` endpoint in Prometheus text format. No external dep needed for counters; for histograms use fixed buckets.
- **First deliverable**: `MetricsMiddleware` + `/metrics` endpoint + tests.

### Gap 5.2 — No distributed tracing hooks
- **Observation**: `RequestIdMiddleware` correlates requests locally but not across services.
- **Recommendation**: Honor `traceparent` (W3C Trace Context) header in `RequestIdMiddleware`, expose `traceId`/`spanId` on `HttpRequest` (e.g. `request.traceContext`). Add a `LoggingMiddleware` enrichment hook to include trace id in log lines.
- **First deliverable**: W3C `traceparent` parse + `request.traceContext` + log enrichment.

### Gap 5.3 — Logging is unstructured strings
- **Observation**: `skr::Logger` uses `{}` format strings; no structured fields.
- **Recommendation**: If skirnir does not already provide structured logging, add a thin `Logger::LogInformation("msg", fields...)` overload that emits JSON or key=value pairs. At minimum, log requests with parseable fields (method, path, status, duration_ms, request_id).
- **First deliverable**: JSON log line option for `LoggingMiddleware`.

---

## 6. Configuration & deployment

### Gap 6.1 — No configuration source abstraction
- **Observation**: All options are constructor arguments; no env/file/config integration.
- **Recommendation**: Provide a simple `IConfiguration` service (env + optional JSON file) shipped as an opt-in extension. Bind to `HttpServerOptions` etc.
- **First deliverable**: New `Baldr.ConfigurationExtension` and example using it.

### Gap 6.2 — No graceful shutdown drain
- **Observation**: SIGINT/SIGTERM calls `Stop()`, but no waiting for in-flight requests to complete.
- **Recommendation**: Track in-flight connections with a counter; on `Stop()` stop accepting new connections, wait up to `shutdownTimeout` (default 30s) for in-flight to finish, then force-close.
- **First deliverable**: Graceful drain + tests.

### Gap 6.3 — No health/readiness endpoints
- **Observation**: Nothing built-in.
- **Recommendation**: Add `app.MapHealthChecks({"/healthz", "/readyz"})` that registers simple endpoints returning 200/503 based on user-supplied predicates (e.g. DB connectivity).
- **First deliverable**: `MapHealthChecks` + example.

---

## 7. API surface & developer experience

### Gap 7.1 — No model validation
- **Observation**: Handlers can return `JsonResult`/`IResult`, but no automatic 400 on invalid input.
- **Recommendation**: Provide a `validate()` trait/concept; allow handlers returning `Result<std::T, ValidationError>` to be mapped to 400 automatically by a `ValidationMiddleware`. Keep it minimal.
- **First deliverable**: `ValidationMiddleware` + a small `Required`/`Range` validator helper.

### Gap 7.2 — No OpenAPI / Swagger generation
- **Observation**: None. Major omission for any "good" web framework today.
- **Recommendation**: Either (a) ship an extension that walks the `Router` and emits an OpenAPI 3.1 document, or (b) document how to integrate a third-party generator. (a) is preferred.
- **First deliverable**: OpenAPI extension reading `RouteOptions::metadata` (see Gap 2.1) + Swagger UI mount example.

### Gap 7.3 — No minimal "baldr new" scaffolding
- **Observation**: Examples exist but no template generator.
- **Recommendation**: Add a `tools/new_project.sh` (or CMake template) that scaffolds a CMake project wiring `FetchContent` for baldr. Document in `docs/get-started.md`.

### Gap 7.4 — No versioning policy for the public API
- **Observation**: `CHANGELOG.md` follows Keep-a-Changelog; no explicit API stability tiers.
- **Recommendation**: Add `docs/community/api-stability.md` marking which headers are public-stable vs. internal (anything under `Baldr::Detail::*` already internal; make that explicit).

---

## 8. Testing & quality

### Gap 8.1 — No fuzz tests for the parser
- **Observation**: Hand-written tests are strong but miss random/malformed inputs.
- **Recommendation**: Add a fuzz harness under `test/fuzz/` using LLVM libFuzzer or a simple property-based test (e.g. rapidcheck). Wire CI to run it nightly.
- **First deliverable**: Fuzz harness for `HttpRequestParser::tryParse`.

### Gap 8.2 — No integration tests against a live server with concurrent connections
- **Observation**: Middleware chain tests exist; full server concurrency is not exercised.
- **Recommendation**: Add a thread-pool based test that spawns many concurrent clients against `HttpServer::Run` and checks for races, leaks, and response integrity.
- **First deliverable**: Concurrent client integration test.

### Gap 8.3 — No coverage / sanitizer CI job
- **Observation**: CI builds on gcc-14, clang, MSVC. No `-fsanitize=address,undefined` or coverage.
- **Recommendation**: Add a sanitizer job (`cmake -S . -B build-asan -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined`) running `ctest`. Add `gcov`/`llvm-cov` job for coverage.
- **First deliverable**: Sanitizer workflow in `.github/workflows/`.

---

## 9. Documentation & community

### Gap 9.1 — No tutorial / "first 15 minutes" guide
- **Observation**: `get-started.md` is brief.
- **Recommendation**: Add a step-by-step tutorial: Hello world → DI → Middleware → StaticFiles → Tests. Mirror in `docs/` and `wiki/`.

### Gap 9.2 — No example showing "real" patterns
- **Observation**: Examples are small and isolated.
- **Recommendation**: Add `examples/TodoApi` showing layered service + repository + validation + OpenAPI extension + tests.

### Gap 9.3 — No benchmarks published
- **Observation**: `benchmarks/wrk/` exists but is not wired into CMake/CI.
- **Recommendation**: Add a CI job that runs wrk against `HelloWorld` and publishes a Markdown report on each release.

---

## Suggested execution order

The plan is implementation-ready only if scope is confirmed. A reasonable ordering for an implementation pass (each is independently shippable):

1. **Quick wins** (low risk, high value):
   - Gap 1.1 (keep-alive cap)
   - Gap 2.3 (405 + Allow)
   - Gap 2.2 (auto-HEAD)
   - Gap 3.7 (configurable exception mapper)
   - Gap 4.1 (ETag / Last-Modified / 304)
2. **Production essentials**:
   - Gap 6.2 (graceful shutdown drain)
   - Gap 4.2 (CompressionMiddleware)
   - Gap 3.3 (SecurityHeadersMiddleware)
   - Gap 5.1 (MetricsMiddleware)
3. **DX**:
   - Gap 2.1 (RouteOptions + metadata)
   - Gap 3.6 (JSON body helper)
   - Gap 4.3 (SPA fallback / options)
   - Gap 6.3 (health endpoints)
4. **Auth / API surface**:
   - Gap 3.1 (ApiKeyMiddleware)
   - Gap 7.2 (OpenAPI extension)
   - Gap 3.5 (streaming responses)
5. **Quality bar**:
   - Gap 8.3 (sanitizer CI)
   - Gap 8.1 (fuzz harness)
   - Gap 5.2 (traceparent)

## Open decisions (need user input before any work begins)

1. **Scope** — implement all gaps, or only the "Quick wins" + "Production essentials" groups? Recommendation: start with Quick wins + Production essentials; revisit.
2. **TLS stance** — confirm keep "no TLS, use reverse proxy" (current stance) vs. start a track to add it. Recommendation: keep current stance.
3. **OpenAPI** — build in-tree or document third-party? Recommendation: in-tree extension reading `RouteOptions::metadata`.
4. **Compression backend** — gzip via zlib only first, or also brotli/zstd? Recommendation: gzip only first to keep deps small.
5. **Breaking changes allowed** — several recommendations (Gap 1.1 option struct, Gap 2.1 `MapRoute` overloads, Gap 3.7 middleware signature) change public API. Should we target next major (`0.16.0`) or stay `0.15.x`? Recommendation: next minor for additive, next major if any signature changes are forced.

## Out of scope (explicit)

- Adding HTTP/2 / HTTP/3 support.
- Adding native TLS.
- Multi-tenant / per-tenant configuration.
- Built-in templating engine.
- ORM / database layer.
- WebSocket / SSE baked into core (SSE can come via `StreamResult` in Gap 3.5; WebSocket should be a separate extension).

## Validation plan

For each implemented gap:

- Add unit tests in `test/src/<Name>Spec.cpp` mirroring existing conventions (`*Spec.cpp`).
- Run `cmake --build build && ctest --test-dir build --output-on-failure`.
- Add docs under `docs/usage/` or `docs/extensions/` per the documentation policy in `AGENTS.md` (kebab-case, one concept per page, code fences with `title=` and `linenums="1"`).
- Build docs with `zensical build --clean` to ensure no broken links.
- Reference new sources via GitHub permalinks in docs (CI will rewrite on deploy).

#	Gap	Status	Evidence
1.1	Keep-alive cap / max-requests	COVERED	HttpServerOptions::maxRequestsPerConnection, enableHttp11KeepAlive enforced in HttpConnection::onMessage
1.2	HTTP/2/3 docs	OPEN	docs/get-started.md / docs/setup/build.md never mention reverse-proxy / HTTP/2/3
1.3	TLS stance / proxy docs	PARTIAL	Code comment exists; no nginx/HAProxy sample or production.md
2.1	RouteOptions + metadata	COVERED	src/Baldr/Http/RouteOptions.hpp, exposed via request.route
2.2	Auto-HEAD for GET	COVERED	Router::matchWithAllow falls back to GET entry
2.3	405 + Allow header	COVERED	Router::MatchResult::allowedMethodsOnPath → Connection.cpp emits 405/Allow
2.4	/__routes debug endpoint	OPEN	Router::Snapshot() exists but not exposed over HTTP; no BALDR_DEBUG_ROUTES
3.1	ApiKeyMiddleware	OPEN	No ApiKeyMiddleware, no docs/extensions/auth.md
3.2	CsrfMiddleware (double-submit)	COVERED	src/Baldr/Middleware/Csrf.hpp
3.3	SecurityHeadersMiddleware	COVERED	src/Baldr/Middleware/SecurityHeaders.hpp
3.4	Per-route max body bytes	PARTIAL	RouteOptions exists; no maxBodyBytes field or pre-dispatch enforcement
3.5	Streaming results	COVERED	IStreamingResult, ChunkedStreamResult, FileStreamResult + chunked send
3.6	JSON body helper	COVERED	FromBody<T>, FromQuery<T>, FromParams<T>
3.7	Configurable exception mapper	COVERED	ExceptionHandlerOptions { mapper, includeDetailsInDev, … }
4.1	ETag / Last-Modified / 304 / Range	PARTIAL	ETag + 304 implemented; Range/206/416 not done
4.2	CompressionMiddleware (gzip)	COVERED	src/Baldr/Middleware/Compression/
4.3	SPA fallback / StaticFilesOptions	OPEN	MapStaticFiles(prefix, rootPath) has no options struct
4.4	MIME customization	OPEN	Hardcoded mimeTypes() map, no extraMimeTypes
5.1	MetricsMiddleware + /metrics	COVERED	src/Baldr/Metrics/, Prometheus text
5.2	W3C traceparent	COVERED	TraceContext.hpp, RequestIdMiddleware, log enrichment, tests
5.3	Structured (JSON) logging	PARTIAL	Plain-text formatted line only; no JSON mode
6.1	IConfiguration / ConfigExtension	OPEN	src/Baldr/Configuration/ directory is empty
6.2	Graceful shutdown drain	COVERED	InFlightTracker, gracefulShutdownTimeoutSeconds, waitDrained
6.3	MapHealthChecks /healthz /readyz	OPEN	Zero references in src/
7.1	Validation middleware + Required/Range	OPEN	No ValidationMiddleware, no validators
7.2	OpenAPI extension	COVERED	Full src/Baldr/OpenApi/, Scalar UI, examples
7.3	baldr new scaffolding	OPEN	No tools/, no template generator
7.4	API stability tiers doc	OPEN	docs/community/ has only contributing + license
8.1	Fuzz harness for parser	OPEN	No test/fuzz/
8.2	Concurrent integration test	OPEN	No multi-client test against live HttpServer::Run
8.3	Sanitizer / coverage CI	OPEN	cmake-multi-platform.yml lacks ASan/UBSan/coverage jobs
9.1	Tutorial / first-15-min guide	PARTIAL	get-started.md stops after Hello World; no progressive DI/middleware/static/tests walkthrough
9.2	TodoApi / Todo CRUD example	COVERED	examples/Todo/ with layered service + repo + OpenAPI + Scalar
9.3	Benchmarks in CI	OPEN	benchmarks/wrk/ is manual only
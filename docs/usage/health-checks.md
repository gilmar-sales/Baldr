# Health checks

`WebApplication::MapHealthChecks` registers `GET` endpoints that return the current health of the process and, optionally, of named dependencies. The intent is to plug straight into Kubernetes-style liveness/readiness probes, plain uptime monitors, or `curl`-based smoke tests.

## Basic usage

```cpp title="src/main.cpp" linenums="1"
app->MapHealthChecks(
    { "/healthz", "/readyz" },
    {
        { "db",    [](const HttpRequest&) { return pingDatabase(); } },
        { "cache", [](const HttpRequest&) { return cacheOk();       } },
    },
    "/livez");
```

Three endpoints are registered:

- `GET /healthz` and `GET /readyz` — run every predicate, return the aggregated result.
- `GET /livez` — unconditional liveness check, always returns `200`.

A successful response (all predicates return `true`) looks like:

```http title="Response"
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 47

{"status":"healthy","checks":{"db":true,"cache":true}}
```

When any predicate returns `false` the status becomes `503 Service Unavailable` and the body shows which check failed:

```json title="Response"
{"status":"unhealthy","checks":{"db":true,"cache":false}}
```

Predicates that throw an exception are treated as `false` (the framework swallows the exception and marks the check unhealthy).

## Endpoint shapes

`MapHealthChecks(paths, checks, livePath)`:

| Parameter | Purpose |
|---|---|
| `paths` | One or more URLs that receive the full check aggregate (typically `/healthz` and `/readyz`). |
| `checks` | Named predicates evaluated for every request. Empty list → endpoint always returns `200`. |
| `livePath` | Optional path registered as an unconditional `200` liveness endpoint. Pass `""` to skip. |

## Predicate contract

Predicates are `std::function<bool(const HttpRequest&)>` invoked **synchronously on the request thread**. They must therefore be cheap and non-blocking. Caching the result of a slow probe (e.g. a database ping) is the recommended pattern — invalidate the cache every few seconds out-of-band.

!!! note
    A predicate that throws is captured by the framework and counts as `false` for the current request, so the endpoint still answers instead of returning `500`.

## Examples

A complete runnable example lives at [`examples/HealthChecks`](https://github.com/gilmar-sales/Baldr/tree/main/examples/HealthChecks). It registers `db` and `cache` checks plus a `/livez` liveness endpoint.

```bash title="terminal"
cmake -S examples/HealthChecks -B build-healthchecks
cmake --build build-healthchecks
./build-healthchecks/HealthChecks
# In another shell:
curl -i localhost:8080/healthz
curl -i localhost:8080/livez
```

## Next steps

- Combine with [Rate limit middleware](../middleware/rate-limit.md) to protect `/healthz` against excessive probing.
- Emit Prometheus metrics from the same predicates in [Metrics middleware](../usage/middleware.md).
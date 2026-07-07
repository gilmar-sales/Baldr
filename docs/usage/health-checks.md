# Health checks

`WebApplication::MapHealthChecks` registers `GET` endpoints that return the current health of the process and, optionally, of named dependencies. The intent is to plug straight into Kubernetes-style liveness/readiness probes, plain uptime monitors, or `curl`-based smoke tests.

Health checks are registered with the DI container as implementations of [`IHealthCheck`](../../api/Application/IHealthCheck.md). When `MapHealthChecks` is called the framework resolves every registered check via `ServiceProvider::GetServices<IHealthCheck>()` once, snapshots each one's name and `Check` callable, and registers them under the requested URLs.

## Basic usage

```cpp title="src/main.cpp" linenums="1"
class DatabaseHealthCheck : public baldr::IHealthCheck
{
  public:
    std::string_view CheckName() const noexcept override { return "db"; }
    baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
    {
        return pingDatabase()
                   ? baldr::HealthCheckResult::Healthy("primary db")
                   : baldr::HealthCheckResult::Unhealthy(
                         "primary db", "ping failed");
    }
};

class CacheHealthCheck : public baldr::IHealthCheck
{
  public:
    std::string_view CheckName() const noexcept override { return "cache"; }
    baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
    {
        if (cacheOk())
            return baldr::HealthCheckResult::Healthy();
        return baldr::HealthCheckResult::Degraded(
            "redis replica", "high miss rate",
            R"({"hitRatio":0.42})");
    }
};

auto builder = skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();
builder.GetServiceCollection()
    ->AddTransient<baldr::IHealthCheck, DatabaseHealthCheck>();
builder.GetServiceCollection()
    ->AddTransient<baldr::IHealthCheck, CacheHealthCheck>();

auto app = builder.Build<baldr::WebApplication>();
app->MapHealthChecks({ "/healthz", "/readyz" }, "/livez");
```

Three endpoints are registered:

- `GET /healthz` and `GET /readyz` — run every `IHealthCheck`, return the aggregated result.
- `GET /livez` — unconditional liveness check, always returns `200`.

A successful response (all checks return `Healthy`) looks like:

```http title="Response"
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "healthy",
  "checks": {
    "db":    { "status": "healthy", "description": "primary db" },
    "cache": { "status": "healthy" }
  }
}
```

When any check returns `Unhealthy` the top-level status becomes `"unhealthy"` and the HTTP code flips to `503 Service Unavailable`. A `Degraded` check keeps the endpoint at `200` but surfaces `"status":"degraded"` per-check:

```http title="Response"
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "healthy",
  "checks": {
    "db":    { "status": "healthy" },
    "cache": {
      "status":      "degraded",
      "description": "redis replica",
      "error":       "high miss rate",
      "data":        { "hitRatio": 0.42 }
    }
  }
}
```

A check that throws an exception is converted to `Unhealthy` automatically; the framework catches it and records `what()` as the `error` field.

## Endpoint shapes

`MapHealthChecks(paths, livePath)`:

| Parameter | Purpose |
|---|---|
| `paths` | One or more URLs that receive the full check aggregate (typically `/healthz` and `/readyz`). |
| `livePath` | Optional path registered as an unconditional `200` liveness endpoint. Pass `""` (default) to skip. |

The check list is whatever `GetServices<IHealthCheck>()` returns at the moment `MapHealthChecks` is called. When no `IHealthCheck` is registered the endpoint always returns `200`.

## IHealthCheck contract

```cpp title="include/Baldr/Application/IHealthCheck.hpp" linenums="1"
class IHealthCheck
{
  public:
    virtual ~IHealthCheck() = default;

    /** @brief Stable identifier surfaced in the JSON body. */
    virtual std::string_view CheckName() const noexcept = 0;

    /** @brief Run the probe. Throw on failure, or return HealthCheckResult. */
    virtual baldr::HealthCheckResult Check(const HttpRequest& request) = 0;
};
```

`Check` returns a [`HealthCheckResult`](../../api/Application/HealthCheckResult.md):

```cpp title="include/Baldr/Application/HealthCheckResult.hpp" linenums="1"
enum class HealthStatus : std::uint8_t { Healthy, Degraded, Unhealthy };

struct HealthCheckResult
{
    HealthStatus                 status;
    std::string                  description;
    std::optional<std::string>   error;
    std::optional<std::string>   data;  // pre-serialized JSON

    static HealthCheckResult Healthy(std::string description = {});
    static HealthCheckResult Degraded(std::string                  description = {},
                                      std::optional<std::string>   error = std::nullopt,
                                      std::optional<std::string>   data  = std::nullopt);
    static HealthCheckResult Unhealthy(std::string                  description = {},
                                       std::optional<std::string>   error = std::nullopt,
                                       std::optional<std::string>   data  = std::nullopt);
};
```

- `CheckName` is part of the public contract of the probe and must be stable across calls. It is surfaced verbatim as the key under `checks` in the response body.
- `Check` runs **synchronously on the request thread** for every probe request and must be cheap and non-blocking. Cache the result of slow upstream calls and return the most recent cached value.
- `data` is a pre-serialized JSON fragment inlined verbatim into the response body — supply `R"({"latencyMs":42})"` or similar from the check. The framework does not parse or re-serialize it.
- Throwing from `Check` is equivalent to returning `Unhealthy({}, ex.what())`.

## Status mapping

| Per-check status | Contribution to top-level status | HTTP status |
|---|---|---|
| `Healthy`   | `"healthy"` | `200` |
| `Degraded`  | `"healthy"` | `200` |
| `Unhealthy` | `"unhealthy"` | `503` |

Top-level status is `"unhealthy"` only when **at least one** check is `Unhealthy`. The per-check `status` is always one of `"healthy"`, `"degraded"`, or `"unhealthy"`.

!!! note "Register checks as Transient"
    The framework resolves `IHealthCheck` instances with `GetServices<IHealthCheck>()`. Register each check as `Transient` (the recommended lifetime for stateless probes). Registering two `IHealthCheck` implementations as `Singleton` collapses them to a single instance because the DI cache is keyed by interface type, not implementation.

!!! tip "Holding state across probes"
    If a probe needs cached state (e.g. a background ping loop), keep that state in a long-lived collaborator and resolve it from the DI container in the check's constructor:

    ```cpp
    class DatabaseHealthCheck : public baldr::IHealthCheck
    {
      public:
        explicit DatabaseHealthCheck(ConnectionPool& pool) : mPool(pool) {}
        std::string_view CheckName() const noexcept override { return "db"; }
        baldr::HealthCheckResult Check(const baldr::HttpRequest&) override
        {
            return mPool.lastPingOk()
                       ? baldr::HealthCheckResult::Healthy()
                       : baldr::HealthCheckResult::Unhealthy({}, "ping failed");
        }
      private:
        ConnectionPool& mPool;
    };
    ```

    Register the pool as a singleton and the check as transient:

    ```cpp
    services->AddSingleton<ConnectionPool>();
    services->AddTransient<baldr::IHealthCheck, DatabaseHealthCheck>();
    ```

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
- Emit Prometheus metrics from the same checks in [Metrics middleware](../usage/middleware.md).
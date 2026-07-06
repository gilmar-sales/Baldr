# Health checks

`WebApplication::MapHealthChecks` registers `GET` endpoints that return the current health of the process and, optionally, of named dependencies. The intent is to plug straight into Kubernetes-style liveness/readiness probes, plain uptime monitors, or `curl`-based smoke tests.

Health checks are registered with the DI container as implementations of [`IHealthCheck`](../../api/Application/IHealthCheck.md). When `MapHealthChecks` is called the framework resolves every registered check via `ServiceProvider::GetServices<IHealthCheck>()` once, snapshots each one's name and `Check` callable, and registers them under the requested URLs.

## Basic usage

```cpp title="src/main.cpp" linenums="1"
class DatabaseHealthCheck : public baldr::IHealthCheck
{
  public:
    std::string_view CheckName() const noexcept override { return "db"; }
    bool Check(const baldr::HttpRequest&) override { return pingDatabase(); }
};

class CacheHealthCheck : public baldr::IHealthCheck
{
  public:
    std::string_view CheckName() const noexcept override { return "cache"; }
    bool Check(const baldr::HttpRequest&) override { return cacheOk(); }
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

A successful response (all checks return `true`) looks like:

```http title="Response"
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 47

{"status":"healthy","checks":{"db":true,"cache":true}}
```

When any check returns `false` the status becomes `503 Service Unavailable` and the body shows which check failed:

```json title="Response"
{"status":"unhealthy","checks":{"db":true,"cache":false}}
```

A check that throws an exception is treated as `false` (the framework swallows the exception and marks the check unhealthy).

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

    /** @brief Run the probe. Return false (or throw) for unhealthy. */
    virtual bool Check(const HttpRequest& request) = 0;
};
```

- `CheckName` is part of the public contract of the probe and must be stable across calls. It is surfaced verbatim as the key under `checks` in the response body.
- `Check` runs **synchronously on the request thread** for every probe request and must be cheap and non-blocking. Cache the result of slow upstream calls and return the most recent cached value.

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
        bool Check(const baldr::HttpRequest&) override { return mPool.lastPingOk(); }
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
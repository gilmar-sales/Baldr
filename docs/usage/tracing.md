# Tracing

Baldr participates in the [W3C Trace Context](https://www.w3.org/TR/trace-context/) standard so a single `traceId` follows a request across services. The implementation lives in [`RequestIdMiddleware`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/RequestId.hpp), which parses the inbound `traceparent` header, mints a new `spanId` for the local hop, and propagates the header on the response.

## The `traceparent` header

```
traceparent: 00-<traceId 32 hex>-<spanId 16 hex>-<flags 2 hex>
```

| Field     | Length  | Notes                                                    |
| --------- | ------- | -------------------------------------------------------- |
| `version` | 2 hex   | `00` today. Future versions are accepted and forwarded.  |
| `traceId` | 32 hex  | All-zero is invalid and triggers regeneration.           |
| `spanId`  | 16 hex  | All-zero is invalid. We always mint a new span id.       |
| `flags`   | 2 hex   | Bit `01` = `sampled`. Other bits are reserved.           |

Malformed headers are treated as if absent: the middleware generates fresh `traceId` / `spanId` values and continues.

!!! tip "Forward compatibility"
    Per the W3C spec, unknown versions must still propagate. Baldr accepts and forwards any version whose first four fields are well-formed.

## `Baldr::TraceContext`

[`src/Baldr/Http/TraceContext.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/TraceContext.hpp) exposes the parsed context on every request:

```cpp title="TraceContext.hpp"
namespace Baldr {
struct TraceContext {
    std::uint8_t version{0};
    std::string  traceId;     // 32 hex chars
    std::string  spanId;      // 16 hex chars
    std::uint8_t traceFlags{0};
    bool         valid{false};

    bool sampled() const noexcept { return (traceFlags & 0x01) != 0; }
};
}
```

After `RequestIdMiddleware` has run, `request.traceContext.valid` is always `true` and downstream middleware, handlers, and the logger can read it directly.

```cpp title="Reading the trace id in a handler"
app->MapGet("/orders/{id}", [](HttpRequest& req, HttpResponse& res) {
    LOG_INFO("handling order {}", req.params["id"]);
    if (req.traceContext.valid) {
        LOG_INFO("trace={} span={}", req.traceContext.traceId,
                 req.traceContext.spanId);
    }
    ...
});
```

## Middleware ordering

`LoggingMiddleware` reads `request.traceContext` to enrich log lines, so `RequestIdMiddleware` **must run before** `LoggingMiddleware`:

```cpp title="src/main.cpp" linenums="1"
app->Use<RequestIdMiddleware>();   // parse + propagate traceparent
app->Use<LoggingMiddleware>();     // reads request.traceContext
```

A log line for a sampled request looks like:

```
Request  - 'HTTP/1.1' 'GET' '/orders/42' trace=0af7651916cd43dd8448eb211c80319c span=b7ad6b7169203331
Response - 200 'GET' '/orders/42' - 1234us - 127.0.0.1 trace=0af7651916cd43dd8448eb211c80319c span=b7ad6b7169203331
```

When the request is not sampled, only `trace=` is appended.

## Options

```cpp title="RequestIdOptions"
struct RequestIdOptions {
    bool propagateTraceparentResponse = true;
    bool useTraceIdAsRequestIdFallback = true;
};
```

| Field                              | Effect                                                                                              |
| ---------------------------------- | --------------------------------------------------------------------------------------------------- |
| `propagateTraceparentResponse`     | When `true` (default), set `traceparent` on the response so downstream HTTP calls can copy it.      |
| `useTraceIdAsRequestIdFallback`    | When `true` (default), populate `X-Request-ID` from the `traceId` if the client did not send one.   |

If a client sends an explicit `X-Request-ID`, it is always preserved on both the request and the response — the middleware never overwrites a caller-supplied id.

```cpp title="Disabling response propagation"
RequestIdOptions opts;
opts.propagateTraceparentResponse = false;
app->Use<RequestIdMiddleware>(opts);
```

## End-to-end example

```bash title="curl with an upstream trace id"
curl -i \
  -H 'traceparent: 00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01' \
  http://localhost:8080/orders/42
```

The response will include both headers, with a fresh `spanId`:

```
HTTP/1.1 200 OK
X-Request-ID: 0af7651916cd43dd8448eb211c80319c
traceparent: 00-0af7651916cd43dd8448eb211c80319c-c4f9d23a12ef0b81-01
```

Copy `traceparent` into the next outgoing HTTP call to stitch a trace across services.

## See also

- [Middleware](middleware.md) — full middleware guide and ordering rules.
- [`RequestIdMiddleware`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/RequestId.hpp) — implementation.
- [`TraceContext`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/TraceContext.hpp) — type and parser.
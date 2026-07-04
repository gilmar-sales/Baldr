# Middleware overview

Baldr ships a set of **middleware** — cross-cutting components that intercept every request. They run in registration order on the way **in** and reverse order on the way **out**; see [Middleware](../usage/middleware.md#order-of-execution) for details.

## Shipped middleware

The middleware headers are header-only and re-exported through `<Baldr/Baldr.hpp>`. Each page documents its options struct.

| Middleware | Purpose | Page |
| --- | --- | --- |
| `LoggingMiddleware` | Logs every request and response with elapsed microseconds | [Logging](logging.md) |
| `RequestIdMiddleware` | Echoes or generates `X-Request-ID` for log correlation | [Request ID](request-id.md) |
| `ExceptionHandlerMiddleware` | Maps thrown exceptions to a 500 response | [Exception handler](exception-handler.md) |
| `CompressionMiddleware` | gzip-encodes eligible response bodies | [Compression](compression.md) |
| `SecurityHeadersMiddleware` | Sets X-Content-Type-Options, X-Frame-Options, HSTS, COOP/CORP, etc. | [Security headers](security-headers.md) |
| `CorsMiddleware` | CORS headers + `OPTIONS` preflight short-circuit | [CORS](cors.md) |
| `CsrfMiddleware` | Double-submit cookie CSRF protection | [CSRF](csrf.md) |
| `RateLimitMiddleware` | Rejects clients that exceed a configured rate | [Rate limit](rate-limit.md) |

## Recommended pipeline order

Outer-most first, inner-most (closest to the handler) last. This matches the order in [Middleware](../usage/middleware.md#order-of-execution):

```cpp title="src/main.cpp"
app->Use<RequestIdMiddleware>()
   ->Use<ExceptionHandlerMiddleware>()
   ->Use<LoggingMiddleware>()
   ->Use<CompressionMiddleware>()
   ->Use<SecurityHeadersMiddleware>()
   ->Use<CorsMiddleware>()
   ->Use<CsrfMiddleware>()
   ->Use<RateLimitMiddleware>();
```

## Writing your own

Any class that implements [`IMiddleware`](../usage/middleware.md#the-imiddleware-interface) can be added to the pipeline:

1. Implement `IMiddleware`.
2. Register the implementation with the service collection (typically `AddTransient<T>()`).
3. Add the middleware with `app->Use<T>()`.

See the [Middleware usage page](../usage/middleware.md#writing-your-own) for a complete walkthrough.
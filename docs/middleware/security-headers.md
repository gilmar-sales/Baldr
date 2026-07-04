# Security headers middleware

`SecurityHeadersMiddleware` writes a standard set of browser-side security headers on every response. Each header is opt-out via `std::optional<std::string>` — set the option to `std::nullopt` to suppress it, or to an empty string to emit the header with no value.

## Enabling it

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->Use<SecurityHeadersMiddleware>();

    app->MapGet("/", [] { return Payload { .message = "ok" }; });

    app->Run();
}
```

`SecurityHeadersMiddleware` is defined in [`src/Baldr/Middleware/SecurityHeaders.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/SecurityHeaders.hpp). It has no constructor dependencies.

## Options

`SecurityHeadersOptions`:

| Field | Default | Header emitted | Notes |
| --- | --- | --- | --- |
| `contentTypeOptions` | `"nosniff"` | `X-Content-Type-Options` | Disable browser MIME sniffing. |
| `frameOptions` | `"DENY"` | `X-Frame-Options` | Set to `"SAMEORIGIN"` to allow same-origin framing. |
| `referrerPolicy` | `"strict-origin-when-cross-origin"` | `Referrer-Policy` | Full Referrer-Policy grammar accepted. |
| `strictTransportSecurity` | `std::nullopt` | `Strict-Transport-Security` | Set explicitly in production, e.g. `"max-age=31536000; includeSubDomains"`. |
| `permissionsPolicy` | `std::nullopt` | `Permissions-Policy` | E.g. `"geolocation=(), microphone=()"`. |
| `crossOriginOpenerPolicy` | `"same-origin"` | `Cross-Origin-Opener-Policy` | |
| `crossOriginResourcePolicy` | `"same-origin"` | `Cross-Origin-Resource-Policy` | Set to `"cross-origin"` for public assets. |
| `crossOriginEmbedderPolicy` | `std::nullopt` | `Cross-Origin-Embedder-Policy` | Required by some isolation features. |

Set a field to `std::nullopt` to **not** emit that header. Set it to an empty string to emit the header with no value.

## Production hardening

```cpp title="src/main.cpp"
SecurityHeadersMiddleware::SecurityHeadersMiddleware(
    SecurityHeadersOptions {
        .strictTransportSecurity     = "max-age=31536000; includeSubDomains",
        .permissionsPolicy           = "geolocation=(), microphone=(), camera=()",
        .crossOriginEmbedderPolicy   = "require-corp",
    })
{}
```

## Where to put it

Register `SecurityHeadersMiddleware` **before** `CorsMiddleware` so the security headers ride along on preflight `OPTIONS` responses too:

```cpp title="src/main.cpp"
app->Use<RequestIdMiddleware>()
   ->Use<ExceptionHandlerMiddleware>()
   ->Use<LoggingMiddleware>()
   ->Use<CompressionMiddleware>()
   ->Use<SecurityHeadersMiddleware>()
   ->Use<CorsMiddleware>()
   ->Use<RateLimitMiddleware>();
```
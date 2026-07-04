# CSRF middleware

`CsrfMiddleware` protects state-changing endpoints from cross-site request forgery using the **double-submit cookie** pattern. The framework issues a per-client token cookie on safe requests; the client must echo the same token in a request header on every unsafe request.

## Enabling it

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->Use<CsrfMiddleware>();

    app->MapGet("/profile", [] { return Payload { .message = "ok" }; });
    app->MapPost("/transfer", [](HttpRequest&) {
        return Payload { .message = "transferred" };
    });

    app->Run();
}
```

`CsrfMiddleware` is defined in [`src/Baldr/Middleware/Csrf.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/Csrf.hpp). It has no constructor dependencies and uses Angular's `XSRF-TOKEN` / `X-XSRF-TOKEN` conventions by default.

## How it works

For each request:

1. The middleware classifies the request method. Methods in `protectedMethods` (default `POST`, `PUT`, `PATCH`, `DELETE`) are **unsafe** and require a valid token. Other methods (`GET`, `HEAD`, `OPTIONS`) are **safe** and may receive a fresh token.
2. For **unsafe** requests:
    - The configured request header must be present and non-empty.
    - The configured cookie must be present and non-empty.
    - The header value and cookie value must match exactly (constant-time comparison).
    - On any failure the middleware writes a `403 Forbidden` plain-text response and short-circuits.
3. For **safe** requests with `issueCookieOnSafeRequest = true`, the middleware issues the cookie if absent. With `issueCookieOnSafeRequest = false`, the caller is responsible for issuing the cookie.
4. Requests whose path begins with a configured `exemptPathPrefixes` entry skip the check on unsafe methods. The cookie is still issued on safe requests when applicable.

The token itself is **not cryptographically secure** — it is generated the same way as `RequestIdMiddleware`'s id. It is suitable for the double-submit pattern, where uniqueness comes from the per-client cookie and the attack model assumes the attacker cannot read the cookie.

!!! tip "Use HTTPS"
    Set `cookieSecure = true` so browsers only send the CSRF cookie over HTTPS, otherwise the double-submit property is weakened.

## Options

`CsrfOptions`:

| Field | Default | Description |
| --- | --- | --- |
| `cookieName` | `"XSRF-TOKEN"` | Name of the CSRF cookie. The default matches Angular's convention. |
| `headerName` | `"X-XSRF-TOKEN"` | Name of the request header the client must echo the token in. The lookup is case-insensitive. |
| `protectedMethods` | `POST, PUT, PATCH, DELETE` | Methods that require a valid token. |
| `exemptPathPrefixes` | `{}` | Path prefixes that bypass the check on unsafe methods. Trailing slash required for prefix match. |
| `issueCookieOnSafeRequest` | `true` | Issue the CSRF cookie on safe requests when missing. |
| `cookieHttpOnly` | `false` | `HttpOnly` flag for the issued cookie. Must be `false` for browser JS to read it. |
| `cookieSecure` | `false` | `Secure` flag for the issued cookie. Enable in production. |
| `cookieMaxAge` | `0` | `Max-Age` (seconds) for the issued cookie. `0` is a session cookie. |

## Example: webhooks exempt

```cpp title="src/main.cpp"
CsrfMiddleware::CsrfMiddleware(
    CsrfOptions {
        .cookieSecure            = true,
        .exemptPathPrefixes      = { "/api/webhooks/" },
    })
{}
```

In this configuration a `POST /api/webhooks/stripe` is allowed without a token (assuming the upstream signature is validated separately), while all other unsafe requests are still protected.

## Where to put it

Register `CsrfMiddleware` **after** `CorsMiddleware` so cross-origin preflight requests can complete before the CSRF check, and **before** `RateLimitMiddleware` so rejected requests are still throttled rather than rejected outright.

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
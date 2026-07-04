# Compression middleware

`CompressionMiddleware` gzip-encodes eligible response bodies on the fly. It runs **after** the handler, inspects the produced response, and rewrites it with `Content-Encoding: gzip` if all eligibility rules pass.

## Enabling it

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().WithExtension<BaldrExtension>();
    auto app = builder.Build<WebApplication>();

    app->Use<ExceptionHandlerMiddleware>()
       ->Use<LoggingMiddleware>()
       ->Use<CompressionMiddleware>();

    app->MapGet("/", [] { return Payload { .message = "Hello, World!" }; });

    app->Run();
}
```

`CompressionMiddleware` is defined in [`src/Baldr/Middleware/Compression/Middleware.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Middleware/Compression/Middleware.hpp). It has no constructor dependencies.

!!! note "Requires zlib"
    Baldr links `ZLIB::ZLIB` unconditionally (`CMakeLists.txt:38`). Consumers must have the zlib development headers installed — see [Build integration](../setup/build.md#platform-notes).

## Eligibility rules

The middleware compresses the response only when **all** of the following hold:

1. The response is **not** a streaming response (chunked transfer encoding is incompatible with on-the-fly rewrites).
2. The response body is non-empty.
3. The response has no pre-existing `Content-Encoding` header.
4. The status code is not `204 No Content` or `304 Not Modified` (the HTTP spec forbids a body on these).
5. The body length is at least `CompressionOptions::minBodyBytes` (default `1024` — compression overhead rarely pays off on tiny bodies).
6. The `Content-Type` starts with one of `CompressionOptions::mimeTypePrefixes` (case-insensitive).
7. The client's `Accept-Encoding` does not contain `gzip;q=0`.

## Options

`CompressionOptions`:

| Field | Default | Description |
| --- | --- | --- |
| `mimeTypePrefixes` | `text/`, `application/json`, `application/javascript`, `application/xml`, `application/xhtml+xml`, `image/svg+xml` | Case-insensitive prefixes matched against the response `Content-Type`. Bodies whose content type does not start with any prefix are not compressed. |
| `minBodyBytes` | `1024` | Minimum body length to compress. Set to `0` to compress everything that passes the other checks. |
| `level` | `-1` | Compression level passed to zlib (`1..9`, or `-1` for zlib's default `6`). Higher values trade CPU for smaller output. |

## Customising the eligible mime types

```cpp title="src/main.cpp"
CompressionMiddleware::CompressionMiddleware(
    CompressionOptions {
        .mimeTypePrefixes = { "text/", "application/json" },
        .minBodyBytes     = 2048,
        .level            = 9,
    })
{}
```

## Where to put it

Register `CompressionMiddleware` after `LoggingMiddleware` so the log line records the **pre-compression** body size — clients see the gzipped size over the wire, but the framework's metrics and logs report what the handler actually produced.

```cpp title="src/main.cpp"
app->Use<RequestIdMiddleware>()
   ->Use<ExceptionHandlerMiddleware>()
   ->Use<LoggingMiddleware>()
   ->Use<CompressionMiddleware>()
   ->Use<CorsMiddleware>();
```
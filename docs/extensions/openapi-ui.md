# Scalar API Reference UI

Baldr ships an interactive [Scalar](https://github.com/scalar/scalar) UI inside the library — no CDN, no runtime files. The bundle is pulled into the translation unit via `std::embed` (C++26, [P1967R14](https://eel.is/c++draft/cpp.embed)) and served from process memory by a one-liner helper.

!!! info "Baldr's Scalar bundle"
    Version **1.62.4** is vendored under `src/Baldr/OpenApi/Assets/` (JS bundle, stylesheet, HTML wrapper, MIT notice). The library is compiled directly with `#embed`, so the bytes live in the same `.text` section as your code and use the same linker treatment.

## Quick start

Add the helper after your routes are registered, then run:

```cpp title="src/main.cpp"
#include <Baldr/Baldr.hpp>

int main()
{
    auto builder =
        skr::ApplicationBuilder()
            .WithExtension<baldr::BaldrExtension>()
            .WithExtension<baldr::BaldrOpenApiExtension>([](auto& ext) {
                baldr::OpenApiOptions opts;
                opts.info.title   = "Devices API";
                opts.info.version = "1.0.0";
                ext.WithOptions(opts);
            });

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/api/devices").Handle([] { /* ... */ });

    baldr::MapScalarUi(*app);
    app->Run();
}
```

On the first request to the UI — or when `app.Run()` hands the `HttpServerOptions` port to the helper at mount time — Baldr logs a line you can ctrl+click in any modern terminal:

```
[Information] 2026-... 'baldr::ScalarUi': Scalar UI listening at http://0.0.0.0:8080/scalar (spec: /openapi.json)
```

![Terminal banner example](https://raw.githubusercontent.com/gilmar-sales/Baldr/main/docs/assets/embedded-ui-banner.png)

The URL picks up the actual `HttpServerOptions::port`, so the line keeps working even when you change the bind port. If you skip `BaldrExtension` and wire the application manually, the helper logs a warning with the mount path instead — still clickable as a relative URL.

## What gets mounted

| Route | Source | Content-Type |
| --- | --- | --- |
| `mountPath` (default `/scalar`) | HTML wrapper with placeholders rewritten against `mountPath`, `specUrl` and `pageTitle` | `text/html; charset=utf-8` |
| `mountPath/scalar-reference.js` | Scalar's UMD bundle (~3.7 MB) | `application/javascript; charset=utf-8` |
| `mountPath/styles.css` | Scalar stylesheet (~289 KB) | `text/css; charset=utf-8` |

The HTML wrapper contains a `<script id="api-reference" data-spec-url="...">` element followed by the Scalar UMD loader. Scalar fetches the spec URL client-side, so the API itself can remain private to your network.

## Customisation

`MapScalarUi` has four parameters:

```cpp
baldr::MapScalarUi(WebApplication& app,
                    std::string mountPath = "/scalar",
                    std::string specUrl    = "/openapi.json",
                    std::string pageTitle  = "API Reference");
```

| Parameter | Default | Notes |
| --- | --- | --- |
| `mountPath` | `/scalar` | Force-prefixed with `/` if missing. Trailing-mount pairs (e.g. `/docs/api`) are accepted. |
| `specUrl` | `/openapi.json` | Path (or URL) the Scalar client fetches. Override when your spec lives behind an authenticated gateway or a different prefix. |
| `pageTitle` | `API Reference` | Goes into the `<title>` element and into your browser tab. |

To switch to a darker brand colour, supply a custom HTML wrapper rather than the built-in one: copy `src/Baldr/OpenApi/Detail/Assets/index.html`, tweak the `<title>` and the `<script data-configuration>` attribute, and serve it from your own static-files route. The Scalar client loads the rest from `/scalar/scalar-reference.js` and `/scalar/styles.css`.

## Architecture

`MapScalarUi` lives in [`src/Baldr/OpenApi/MapScalarUi.{hpp,cpp}`](https://github.com/gilmar-sales/Baldr/tree/main/src/Baldr/OpenApi/MapScalarUi.hpp). The bytes are pulled in at compile time from the same translation unit that mounts the routes:

```cpp title="src/Baldr/OpenApi/MapScalarUi.cpp"
inline constexpr unsigned char kScalarReferenceJs[] = {
#embed "Assets/scalar-reference.js"
};
```

- **`#embed`** drops the file's bytes inline as a `constexpr` array. gcc-15+ (gcc-16 is the CI compiler) implements the feature, so no CMake generator, no `xxd`, no `objcopy` — just normal C++ you can read in the editor.
- Each asset has a matching `kScalar*Size` constant (just `sizeof(...)`), made available alongside for completeness.
- `OpenApi::EmbeddedScalar::AsStringView` wraps each byte array in a non-owning `std::string_view` because `ContentResult` takes a `std::string body`. The view's source is the `#embed` array, which has program lifetime.

## When not to use it

- **You already serve a UI from another process.** `MapScalarUi` collides only on the prefix you pass as `mountPath`; pick something else or skip the helper.
- **You want a different UI (Swagger UI, Redoc, Stoplight Elements).** Use the same pattern with your own asset bundle — the helper is intentionally a single function so it's easy to fork, and the build system does not require any change.
- **Your bundler is too strict to allow `#embed`.** Compile the file as a `.cpp` translation unit under `-std=c++26` (already enforced by `CMakeLists.txt:9`). No flags or pragmas are required.

## Limitations

- The Scalar bundle is 3.7 MB; it adds that much to your binary's size. Strip it with `strip -s`/`-R .comment --strip-unneeded` post-build if you care.
- The library does not deduplicate the bundle between translation units. The compiler's linker can drop it if no TU references `MapScalarUi`, but every call embeds it once at most.
- The HTML wrapper is a static template. If you want to inject a navigation bar, switch themes, or pass extra `<meta>` tags, copy the template into your own example and pass an override — see [Customisation](#customisation).
- Scalar's standalone bundle evaluates its data-spec URL on the client. If `/openapi.json` requires an `Authorization` header, configure your CORS / auth middleware to allow the browser request through.

## Next steps

- Browse the [`OpenApiExample`](https://github.com/gilmar-sales/Baldr/tree/main/examples/OpenApiExample) program for end-to-end usage.
- Read [OpenAPI extension](openapi.md) for spec options and the JSON Schema dialect.
- See [`MapScalarUi.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/OpenApi/MapScalarUi.hpp) for the implementation.
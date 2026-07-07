# Static files example

[`examples/StaticFiles`](https://github.com/gilmar-sales/Baldr/tree/main/examples/StaticFiles) serves a `wwwroot/` directory under `/static` using `MapStaticFiles`, plus a hand-written landing page at `/`.

## Source

[`examples/StaticFiles/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/StaticFiles/src/main.cpp):

```cpp title="examples/StaticFiles/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

std::filesystem::path exeDir()
{
#if defined(_WIN32)
    return std::filesystem::path(_pgmptr).parent_path();
#else
    if (auto* argv0 = std::getenv("_"))
        return std::filesystem::path(argv0).parent_path();
    return std::filesystem::current_path();
#endif
}

std::filesystem::path resolveWebRoot()
{
    const auto nextToExe =
        std::filesystem::weakly_canonical(exeDir()) / "wwwroot";
    if (std::filesystem::is_directory(nextToExe))
        return nextToExe;

    return std::filesystem::current_path() / "wwwroot";
}

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    const std::filesystem::path webRoot = resolveWebRoot();

    app->MapGet("/", [](baldr::HttpRequest&, baldr::HttpResponse&) {
        return baldr::ContentResult(
            "<!doctype html><meta charset=\"utf-8\">"
            "<title>Static files</title>"
            "<h1>Baldr static-files example</h1>"
            "<ul>"
            "<li><a href=\"/static/index.html\">/static/index.html</a></li>"
            "<li><a href=\"/static/css/site.css\">/static/css/site.css</a></li>"
            "<li><a "
            "href=\"/static/assets/app.js\">/static/assets/app.js</a></li>"
            "<li><a "
            "href=\"/static/assets/img/logo.svg\">/static/assets/img/logo.svg</"
            "a></"
            "li>"
            "<li><a "
            "href=\"/static/assets/hello.txt\">/static/assets/hello.txt</a></"
            "li>"
            "</ul>",
            "text/html",
            baldr::StatusCode::OK);
    });

    app->MapStaticFiles("/static", webRoot.string());

    app->Run();

    return 0;
}
```

## What it shows

- Locating the web root next to the executable at runtime (Windows uses `_pgmptr`, POSIX reads the `_` environment variable), falling back to the working directory.
- Mounting `MapStaticFiles("/static", webRoot.string())`.
- Serving a hand-written `baldr::ContentResult` at `/` so users can browse the asset list.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/StaticFiles
```

In another terminal:

```bash
curl http://localhost:8080/
curl http://localhost:8080/static/index.html
```

## Next steps

- See [Static files](../../usage/static-files.md) for path-safety, MIME-type inference, and streaming behaviour.
- Browse [all examples](../examples.md).

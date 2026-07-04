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
    auto builder = skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

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
            "href=\"/static/assets/img/logo.svg\">/static/assets/img/logo.svg</a></"
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

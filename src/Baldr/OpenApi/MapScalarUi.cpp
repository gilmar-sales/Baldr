#include "MapScalarUi.hpp"

#include <Baldr/Detail/Namespace.hpp>

namespace BALDR_NAMESPACE
{

    /// Scalar 1.x standalone JS bundle (`@scalar/api-reference`).
    const unsigned char kScalarReferenceJs[] = {
#embed "Assets/scalar-reference.js"
    };
    std::size_t kScalarReferenceJsSize = sizeof(kScalarReferenceJs);

    /// Scalar stylesheet (Tailwind v4 build).
    const unsigned char kStylesCss[] = {
#embed "Assets/styles.css"
    };
    std::size_t kStylesCssSize = sizeof(kStylesCss);

    /// HTML wrapper served at @c mountPath that bootstraps Scalar.
    const unsigned char kIndexHtml[] = {
#embed "Assets/index.html"
    };
    std::size_t kIndexHtmlSize = sizeof(kIndexHtml);

    void MapScalarUi(WebApplication& app,
                     std::string     mountPath,
                     std::string     specUrl,
                     std::string     pageTitle)
    {
        if (mountPath.empty() || mountPath.front() != '/')
        {
            mountPath = "/" + mountPath;
        }

        auto sp = app.GetRootServiceProvider();

        const std::string title    = std::move(pageTitle);
        const std::string jsUrl    = mountPath + "/scalar-reference.js";
        const std::string cssUrl   = mountPath + "/styles.css";
        const std::string specPath = std::move(specUrl);
        const std::string jsCt     = "application/javascript; charset=utf-8";
        const std::string cssCt    = "text/css; charset=utf-8";
        const std::string htmlCt   = "text/html; charset=utf-8";

        const std::string jsAsset(
            OpenApi::EmbeddedScalar::AsStringView(kScalarReferenceJs));
        const std::string cssAsset(
            OpenApi::EmbeddedScalar::AsStringView(kStylesCss));
        const std::string htmlTemplate(
            OpenApi::EmbeddedScalar::AsStringView(kIndexHtml));

        app.MapGet(mountPath + "/scalar-reference.js",
                   [jsAsset, jsCt]() { return ContentResult(jsAsset, jsCt); });
        app.MapGet(mountPath + "/styles.css", [cssAsset, cssCt]() {
            return ContentResult(cssAsset, cssCt);
        });
        app.MapGet(
            mountPath,
            [htmlTemplate, title, jsUrl, cssUrl, htmlCt, specPath]() {
                std::string body       = htmlTemplate;
                auto        replaceAll = [&body](const std::string& from,
                                                 const std::string& to) {
                    std::string::size_type pos = 0;
                    while ((pos = body.find(from, pos)) != std::string::npos)
                    {
                        body.replace(pos, from.size(), to);
                        pos += to.size();
                    }
                };
                replaceAll("__TITLE__", title);
                replaceAll("__SPEC_URL__", specPath);
                replaceAll("__JS_URL__", jsUrl);
                replaceAll("__STYLES_URL__", cssUrl);
                // Scalar parses `data-configuration` verbatim as JSON; an
                // empty object (`{}`) gives the default "classic" theme.
                replaceAll("__CONFIGURATION__", "{}");
                return ContentResult(std::move(body), htmlCt);
            });

        if (auto loggerOpt = sp->TryGetService<skr::Logger<ScalarUi>>())
        {
            auto& logger = *loggerOpt;
            if (auto optsOpt = sp->TryGetService<HttpServerOptions>())
            {
                const auto& opts = *optsOpt;
                logger->LogInformation(
                    "Scalar UI listening at http://0.0.0.0:{}{} (spec: {})",
                    opts->port, mountPath, specPath);
            }
            else
            {
                logger->LogWarning(
                    "Scalar UI mounted at {} (spec: {}); could not resolve "
                    "HttpServerOptions for full URL logging - register "
                    "baldr::BaldrExtension to get a ctrl+clickable "
                    "http://host:port URL.",
                    mountPath, specPath);
            }
        }
    }
} // namespace BALDR_NAMESPACE
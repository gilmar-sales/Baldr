#pragma once

#include <Baldr/Baldr.hpp>

#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/ServerOptions.hpp>

#include <Skirnir/Skirnir.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Sentinel type used to tag the UI's own logger category.
     *
     * Baldr registers @c skr::Logger<ScalarUi> as a transient in
     * @c BaldrExtension, mirroring the existing
     * @c skr::Logger<WebApplication> registration.
     */
    struct ScalarUi
    {
    };

    /**
     * @brief Compile-time byte arrays for the vendored Scalar assets.
     *
     * Each asset is pulled in with @c std::embed (C++26, P1967R14)
     * directly into a brace-enclosed @c const unsigned char array; gcc-15+
     * supports this. We then build @c std::string_view s over the same
     * bytes on first use, since @c ContentResult requires a
     * @c std::string body and the assets are plain UTF-8.
     *
     * The bytes live forever (program lifetime), so the views are
     * safely passed by value into route handler closures.
     */
    namespace OpenApi::EmbeddedScalar
    {
        /// Scalar 1.x standalone JS bundle (`@scalar/api-reference`).
        const unsigned char kScalarReferenceJs[] = {
#embed                                                                         \
    "/home/gilmar/dev/Baldr/src/Baldr/OpenApi/Detail/Assets/scalar-reference.js"
        };
        inline constexpr std::size_t kScalarReferenceJsSize =
            sizeof(kScalarReferenceJs);

        /// Scalar stylesheet (Tailwind v4 build).
        const unsigned char kStylesCss[] = {
#embed "/home/gilmar/dev/Baldr/src/Baldr/OpenApi/Detail/Assets/styles.css"
        };
        inline constexpr std::size_t kStylesCssSize = sizeof(kStylesCss);

        /// HTML wrapper served at @c mountPath that bootstraps Scalar.
        const unsigned char kIndexHtml[] = {
#embed "/home/gilmar/dev/Baldr/src/Baldr/OpenApi/Detail/Assets/index.html"
        };
        inline constexpr std::size_t kIndexHtmlSize = sizeof(kIndexHtml);

        /**
         * @brief Translate a compile-time byte array to a UTF-8
         *        @c std::string_view .
         *
         * The vendored assets are plain bytes; this is the single
         * point where we reinterpret them as a string. The view is
         * backed by a @c constexpr array with program lifetime.
         *
         * @tparam N Element count of the byte array.
         * @param data The array reference.
         * @return A non-owning view over the same bytes.
         */
        template <std::size_t N>
        inline constexpr std::string_view AsStringView(
            const unsigned char (&data)[N]) noexcept
        {
            return { reinterpret_cast<const char*>(data), N };
        }

    } // namespace OpenApi::EmbeddedScalar

    /**
     * @brief Mount the embedded Scalar UI at @p mountPath, pointed at
     *        @p specUrl, and log a ctrl+clickable URL to the
     *        @c skr::Logger<ScalarUi> category.
     *
     * The helper:
     *  - serves each vendored Scalar asset (JS bundle, stylesheet, HTML
     *    wrapper) from compile-time memory via routes under
     *    @p mountPath;
     *  - rewrites the @c __SPEC_URL__ placeholder inside the HTML
     *    wrapper with @p specUrl;
     *  - resolves @c HttpServerOptions from the @c WebApplication root
     *    service provider (registered by @c BaldrExtension) and emits
     *    @c "Scalar UI listening at http://0.0.0.0:{port}{mountPath}
     *    (spec: {specUrl})" via @c LogInformation so terminals that
     *    auto-link @c http(s):// URLs let the developer ctrl+click the
     *    line to open the UI;
     *  - falls back to @c LogWarning with the mount path + spec URL
     *    when @c HttpServerOptions is not registered (e.g. when the
     *    caller skipped @c BaldrExtension and used @c MapOpenApi
     *    imperatively).
     *
     * Asset bytes are imported with @c std::embed (C++26, P1967R14)
     * from the vendored files under
     * @c src/Baldr/OpenApi/Detail/Assets/ .
     *
     * @param app       Target application.
     * @param mountPath URL prefix under which the UI is mounted
     *                  (default @c "/scalar"). Must be a non-empty path
     *                  starting with @c '/'.
     * @param specUrl   URL the UI will fetch for the OpenAPI document
     *                  (default @c "/openapi.json").
     * @param pageTitle Title rendered into the HTML page
     *                  (default @c "API Reference").
     */
    inline void MapScalarUi(WebApplication& app,
                            std::string     mountPath = "/scalar",
                            std::string     specUrl   = "/openapi.json",
                            std::string     pageTitle = "API Reference")
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

        const std::string jsAsset(OpenApi::EmbeddedScalar::AsStringView(
            OpenApi::EmbeddedScalar::kScalarReferenceJs));
        const std::string cssAsset(OpenApi::EmbeddedScalar::AsStringView(
            OpenApi::EmbeddedScalar::kStylesCss));
        const std::string htmlTemplate(OpenApi::EmbeddedScalar::AsStringView(
            OpenApi::EmbeddedScalar::kIndexHtml));

        app.MapGet(mountPath + "/scalar-reference.js",
                   [jsAsset, jsCt]() { return ContentResult(jsAsset, jsCt); });
        app.MapGet(mountPath + "/styles.css", [cssAsset, cssCt]() {
            return ContentResult(cssAsset, cssCt);
        });
        app.MapGet(mountPath, [htmlTemplate, title, jsUrl, cssUrl, htmlCt,
                               specPath]() {
            std::string body = htmlTemplate;
            auto        replaceAll =
                [&body](const std::string& from, const std::string& to) {
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
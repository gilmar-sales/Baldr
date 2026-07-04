/**
 * @file OpenApi/BaldrOpenApiExtension.hpp
 * @brief Skirnir extension that registers the OpenAPI spec service and
 *        mounts the spec endpoint on the @c WebApplication.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Baldr.hpp>

#include "OpenApiOptions.hpp"
#include "OpenApiSpecService.hpp"

namespace BALDR_NAMESPACE
{

    /**
     * @brief Plug-and-play Skirnir extension that exposes the Baldr router
     *        as an OpenAPI 3.0.3 document.
     *
     * Register on the @c skr::ApplicationBuilder before constructing the
     * host:
     * @code
     * builder.AddExtension<BaldrOpenApiExtension>()
     *        .WithOptions({ .mountPath = "/openapi.json" });
     * @endcode
     */
    class BaldrOpenApiExtension : public skr::IExtension
    {
      public:
        /// @brief Construct with default options.
        explicit BaldrOpenApiExtension() {}

        /**
         * @brief Register @ref OpenApiSpecService in the DI container.
         */
        void ConfigureServices(skr::ServiceCollection& s) override
        {
            s.AddSingleton<OpenApiSpecService>(
                skr::MakeArc<OpenApiSpecService>(mOptions));
        }

        /**
         * @brief Mount the spec endpoint on the @c WebApplication, when one
         *        exists in the provider graph.
         */
        void UseServices(skr::ServiceProvider& sp) override
        {
            auto specSvc = sp.GetService<OpenApiSpecService>();
            if (!specSvc || !specSvc->options().enabled)
                return;

            auto webApp = sp.GetService<WebApplication>();
            if (!webApp)
                return;

            const std::string mountPath   = specSvc->options().mountPath;
            const std::string contentType = "application/openapi+json";

            webApp->MapGet(
                mountPath,
                [specSvc, router = sp.GetService<Router>(), contentType]() {
                    return ContentResult(specSvc->Cached(router), contentType);
                });
        }

        /**
         * @brief Override the options used by the extension.
         *
         * @return Reference to @c *this for fluent chaining on the builder.
         */
        BaldrOpenApiExtension& WithOptions(OpenApiOptions options = {})
        {
            mOptions = options;
            return *this;
        }

      private:
        OpenApiOptions mOptions;
    };

} // namespace BALDR_NAMESPACE
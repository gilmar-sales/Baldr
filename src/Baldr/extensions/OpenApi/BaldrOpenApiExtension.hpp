#pragma once

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Baldr.hpp>

#include "OpenApiOptions.hpp"
#include "OpenApiSpecService.hpp"

namespace Baldr::OpenApi
{
    // skr::IExtension that wires the OpenAPI spec service and mounts a
    // GET handler at `OpenApiOptions::mountPath`. Consumers opt in via
    // `skr::ApplicationBuilder().WithExtension<BaldrOpenApiExtension>()`.
    class BaldrOpenApiExtension : public skr::IExtension
    {
      public:
        explicit BaldrOpenApiExtension() {}

        void ConfigureServices(skr::ServiceCollection& s) override
        {
            s.AddSingleton<OpenApiSpecService>(
                skr::MakeArc<OpenApiSpecService>(mOptions));
        }

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
                [specSvc, contentType](const skr::Arc<Router>& router) {
                    return ContentResult(specSvc->Cached(router), contentType);
                });
        }

        BaldrOpenApiExtension& WithOptions(OpenApiOptions options = {})
        {
            mOptions = options;
            return *this;
        }

      private:
        OpenApiOptions mOptions;
    };
} // namespace Baldr::OpenApi
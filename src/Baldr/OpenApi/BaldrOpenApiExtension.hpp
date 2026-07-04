#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Baldr.hpp>

#include "OpenApiOptions.hpp"
#include "OpenApiSpecService.hpp"

namespace BALDR_NAMESPACE {

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
            [specSvc, router = sp.GetService<Router>(), contentType]() {
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

} // namespace BALDR_NAMESPACE
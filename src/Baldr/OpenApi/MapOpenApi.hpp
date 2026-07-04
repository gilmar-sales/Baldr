#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Baldr.hpp>

#include "BaldrOpenApiExtension.hpp"
#include "OpenApiSpecService.hpp"

namespace BALDR_NAMESPACE {

// Convenience: mounts the OpenAPI spec at the configured path on the
// given WebApplication. Intended for users who haven't opted into the
// extension but want to register it imperatively from main().
inline void MapOpenApi(WebApplication& app, OpenApiOptions options = {})
{
    auto specSvc = skr::MakeArc<OpenApiSpecService>(std::move(options));
    const std::string mountPath   = specSvc->options().mountPath;
    const std::string contentType = "application/openapi+json";

    app.MapGet(mountPath, [specSvc, &app, contentType]() {
        return ContentResult(specSvc->Cached(app.GetRouter()), contentType);
    });
}

} // namespace BALDR_NAMESPACE
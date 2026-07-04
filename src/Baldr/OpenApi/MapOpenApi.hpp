/**
 * @file OpenApi/MapOpenApi.hpp
 * @brief Imperative helper that mounts the OpenAPI spec on a
 *        @c WebApplication without registering the Skirnir extension.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Baldr.hpp>

#include "BaldrOpenApiExtension.hpp"
#include "OpenApiSpecService.hpp"

namespace BALDR_NAMESPACE
{

    /**
     * @brief Mount the OpenAPI spec at the configured path on @p app.
     *
     * Intended for users who haven't opted into @ref BaldrOpenApiExtension
     * but still want to expose the spec imperatively from @c main().
     *
     * @param app     Target application.
     * @param options Options (mount path, info object, enable flag).
     */
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
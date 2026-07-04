/**
 * @file OpenApi/OpenApiOptions.hpp
 * @brief User-facing options for the OpenAPI spec extension.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>

namespace BALDR_NAMESPACE {

/**
 * @brief OpenAPI @c info object metadata.
 */
struct Info
{
    /// API title rendered into the spec's @c info.title.
    std::string                title   = "Baldr API";
    /// API version rendered into the spec's @c info.version.
    std::string                version = "0.15.1";
    /// Optional description rendered into @c info.description.
    std::optional<std::string> description;
};

/**
 * @brief Options accepted by @ref MapOpenApi and
 *        @ref BaldrOpenApiExtension::WithOptions.
 */
struct OpenApiOptions
{
    /// HTTP path where the rendered spec document is served.
    std::string mountPath = "/openapi.json";
    /// Metadata rendered into the spec's @c info object.
    Info        info;
    /// When @c false, the extension does not register a spec endpoint.
    bool        enabled = true;
};

} // namespace BALDR_NAMESPACE
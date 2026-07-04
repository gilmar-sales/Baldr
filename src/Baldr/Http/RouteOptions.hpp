/**
 * @file Http/RouteOptions.hpp
 * @brief OpenAPI-aligned metadata attached to each registered route.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Baldr/Http/Method.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief OpenAPI metadata describing a single route operation.
     *
     * Populated through the @ref RouteRegistration builder and consumed by
     * the OpenAPI extension when rendering the spec document. Fields
     * mirror the OpenAPI 3 Operation Object.
     */
    struct RouteOptions
    {
        std::optional<std::string> summary;     ///< Operation summary.
        std::optional<std::string> description; ///< Operation description.
        std::vector<std::string>   tags;        ///< Operation tags.
        std::optional<std::string>
                                 operationId; ///< Unique operation identifier.
        bool                     deprecated = false; ///< Mark as deprecated.
        std::vector<std::string> consumes; ///< Accepted request MIME types.
        std::vector<std::string> produces; ///< Emitted response MIME types.
        std::unordered_map<std::string, std::string>
            metadata; ///< Free-form key/value bag for extensions.
    };

    /**
     * @brief Resolved identity of a route as stored on the request.
     *
     * Carries the template path, group prefix, HTTP method and any
     * options the handler may need at runtime (e.g. for OpenAPI-aware
     * responses).
     */
    struct RouteInfo
    {
        std::string  path;  ///< Resolved template path (e.g. @c "/users/{id}").
        std::string  group; ///< Group prefix, if any.
        HttpMethod   method { HttpMethod::Get }; ///< HTTP method.
        RouteOptions options; ///< Copy of the route's OpenAPI options.
    };

} // namespace BALDR_NAMESPACE
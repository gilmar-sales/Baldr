/**
 * @file Application/RouteListing.hpp
 * @brief Debug-only HTTP endpoint that dumps every registered route as JSON.
 *
 * The endpoint is compiled out of release builds. It is gated by the
 * standard @c NDEBUG macro so any consumer building with @c -DNDEBUG (the
 * default for @c CMake 'Release' configurations) never links the code.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <vector>

#include <Baldr/Http/Router.hpp>

namespace BALDR_NAMESPACE
{

#ifndef NDEBUG
    /**
     * @brief Render @c Router::Snapshot() as a JSON document.
     *
     * Used by the @c EnableRouteListing debug endpoint. Output shape:
     * @code
     * {
     *   "routes": [
     *     { "method": "GET",  "path": "/users/{id}", "group": "",
     *       "metadata": {} },
     *     { "method": "POST", "path": "/users",      "group": "",
     *       "metadata": { "requestSchemaJson": "{...}" } }
     *   ]
     * }
     * @endcode
     *
     * Only available in debug builds (@c NDEBUG not defined).
     *
     * @param entries Router snapshot, typically obtained via
     *                @c Router::Snapshot().
     * @return JSON string ready to be returned as a response body.
     */
    std::string RouteListingToJson(const std::vector<RouteEntry>& entries);
#endif

} // namespace BALDR_NAMESPACE
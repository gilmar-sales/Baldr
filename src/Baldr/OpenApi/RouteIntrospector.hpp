/**
 * @file OpenApi/RouteIntrospector.hpp
 * @brief Helpers for translating between Baldr route representations
 *        and OpenAPI path / method strings.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <vector>

#include <Baldr/Http/Router.hpp>

namespace BALDR_NAMESPACE
{
    /**
     * @brief Translate a router path template like @c "/users/:id" (or
     *        @c "/a/**") to OpenAPI path templating (@c "/users/{id}",
     *        @c "/a/{filepath}").
     */
    std::string TranslatePath(const std::string& routerPath);

    /**
     * @brief Deduplicate the union of route templates, preserving the order
     *        in which they were registered.
     */
    std::vector<std::string> UniquePaths(
        const std::vector<RouteEntry>& entries);

} // namespace BALDR_NAMESPACE
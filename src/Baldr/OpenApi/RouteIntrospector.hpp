#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <vector>

#include <Baldr/Http/Router.hpp>

namespace BALDR_NAMESPACE {

// Maps an HttpMethod enum to its lowercase OpenAPI verb.
const char* MethodToString(HttpMethod m);

// Translates a router path template like `/users/:id` (or `/a/**`) to
// OpenAPI path templating (`/users/{id}`, `/a/{filepath}`).
std::string TranslatePath(const std::string& routerPath);

// Deduplicates the union of route templates, preserving the order in
// which they were registered.
std::vector<std::string> UniquePaths(const std::vector<RouteEntry>& entries);

} // namespace BALDR_NAMESPACE
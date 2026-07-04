/**
 * @file OpenApi/OpenApiSpecService.hpp
 * @brief Caches the rendered OpenAPI document and rebuilds it on demand.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/Router.hpp>

#include "JsonSchemaEmitter.hpp"
#include "OpenApiOptions.hpp"
#include "SpecBuilder.hpp"

namespace BALDR_NAMESPACE {

/**
 * @brief Builds and caches the OpenAPI document from a live @ref Router.
 *
 * The first call to @ref Cached renders and stores the spec; subsequent
 * calls return the cached string. Call @ref Regenerate to force a rebuild
 * (typically after routes have been registered dynamically).
 */
class OpenApiSpecService
{
  public:
    /**
     * @brief Construct with the desired options.
     */
    explicit OpenApiSpecService(OpenApiOptions opts) : mOptions(std::move(opts))
    {
    }

    /**
     * @brief Return the cached spec, rendering it on first access.
     *
     * @param router The router whose routes are introspected.
     * @return Reference to the cached JSON document string.
     */
    const std::string& Cached(const skr::Arc<Router>& router);

    /**
     * @brief Force a rebuild of the cached spec on the next access.
     */
    void Regenerate(const skr::Arc<Router>& router);

    /// @return The options this service was constructed with.
    const OpenApiOptions& options() const { return mOptions; }

  private:
    OpenApiOptions mOptions;
    SchemaRegistry mRegistry;
    std::string    mCache;
    bool           mRendered { false };
};

} // namespace BALDR_NAMESPACE
#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/Router.hpp>

#include "JsonSchemaEmitter.hpp"
#include "OpenApiOptions.hpp"
#include "SpecBuilder.hpp"

namespace BALDR_NAMESPACE {

class OpenApiSpecService
{
  public:
    explicit OpenApiSpecService(OpenApiOptions opts) : mOptions(std::move(opts))
    {
    }

    const std::string& Cached(const skr::Arc<Router>& router);

    void Regenerate(const skr::Arc<Router>& router);

    const OpenApiOptions& options() const { return mOptions; }

  private:
    OpenApiOptions mOptions;
    SchemaRegistry mRegistry;
    std::string    mCache;
    bool           mRendered { false };
};

} // namespace BALDR_NAMESPACE
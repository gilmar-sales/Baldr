#pragma once

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Router.hpp>

#include "JsonSchemaEmitter.hpp"
#include "OpenApiOptions.hpp"
#include "SpecBuilder.hpp"

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
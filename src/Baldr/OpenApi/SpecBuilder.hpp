#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include <Baldr/Http/Router.hpp>

#include "JsonSchemaEmitter.hpp"
#include "OpenApiOptions.hpp"

namespace BALDR_NAMESPACE
{

    // Renders an OpenAPI 3.0.3 JSON document from the router snapshot
    // plus a registry of named JSON Schema components.
    class SpecBuilder
    {
      public:
        explicit SpecBuilder(OpenApiOptions opts) : mOptions(std::move(opts)) {}

        SpecBuilder& SetRegistry(const SchemaRegistry& reg)
        {
            mRegistry = &reg;
            return *this;
        }

        std::string Render(const std::vector<RouteEntry>& entries);

      private:
        OpenApiOptions        mOptions;
        const SchemaRegistry* mRegistry { nullptr };
    };

} // namespace BALDR_NAMESPACE
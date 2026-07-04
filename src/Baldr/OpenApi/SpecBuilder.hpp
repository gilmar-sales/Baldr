/**
 * @file OpenApi/SpecBuilder.hpp
 * @brief Renders an OpenAPI 3.0.3 JSON document from a router snapshot
 *        plus a registry of named JSON Schema components.
 */

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

    /**
     * @brief Assembles the final OpenAPI document from introspection
     *        inputs.
     */
    class SpecBuilder
    {
      public:
        /**
         * @brief Construct with the desired options.
         */
        explicit SpecBuilder(OpenApiOptions opts) : mOptions(std::move(opts)) {}

        /**
         * @brief Attach the schema registry used to populate
         *        @c components.schemas.
         */
        SpecBuilder& SetRegistry(const SchemaRegistry& reg)
        {
            mRegistry = &reg;
            return *this;
        }

        /**
         * @brief Render the OpenAPI 3.0.3 JSON document for @p entries.
         */
        std::string Render(const std::vector<RouteEntry>& entries);

      private:
        OpenApiOptions        mOptions;
        const SchemaRegistry* mRegistry { nullptr };
    };

} // namespace BALDR_NAMESPACE
#pragma once

#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Baldr::OpenApi
{
    // A registry that dedupes schema definitions by type identifier. Two
    // routes using the same `T` share one entry under
    // `components.schemas/<name>` and reference it via `$ref`.
    class SchemaRegistry
    {
      public:
        // Returns true if the type was newly registered; false if it was
        // already present (and the existing schema string is unchanged).
        bool Register(std::string_view typeName, std::string schema);

        [[nodiscard]] bool Contains(std::string_view typeName) const;

        // Renders the contents of `components.schemas` as a JSON object.
        [[nodiscard]] std::string RenderComponents() const;

        [[nodiscard]] const std::unordered_map<std::string, std::string>&
        Schemas() const
        {
            return mSchemas;
        }

      private:
        std::unordered_map<std::string, std::string> mSchemas;
    };

    namespace Detail
    {
        // Returns the draft-07 `type` keyword for one of the supported
        // primitive fields. Specialised below.
        template <typename T>
        const char* PrimitiveTypeName();

        template <typename T>
        struct IsSupportedField
            : std::bool_constant<std::is_same_v<T, std::string> ||
                                 std::is_same_v<T, std::string_view> ||
                                 std::is_integral_v<T> ||
                                 std::is_same_v<T, double> ||
                                 std::is_same_v<T, float> ||
                                 std::is_same_v<T, bool>>
        {
        };
    } // namespace Detail

    // Builds a draft-07 schema for a reflectable struct. Members whose
    // type is not in the supported set trigger a compile-time error.
    template <typename T>
    std::string EmitStructSchema()
    {
        static_assert(std::is_class_v<T>,
                      "EmitStructSchema<T>: T must be a class type");

        std::string out;
        out += "{\"$schema\":\"http://json-schema.org/draft-07/schema#\",";
        out += "\"type\":\"object\",\"properties\":{";

        std::vector<std::string> required;

        bool first = true;
        template for (constexpr auto member :
                      std::define_static_array(
                          std::meta::nonstatic_data_members_of(
                              ^^T,
                              std::meta::access_context::current())))
        {
            constexpr auto name = std::meta::identifier_of(member);
            T                 obj {};
            using FieldT = std::remove_cvref_t<decltype(obj.[:member:])>;

            static_assert(
                Detail::IsSupportedField<FieldT>::value,
                "EmitStructSchema<T>: member has an unsupported type "
                "for auto-introspection; declare a JSON Schema string "
                "explicitly via WithResponseSchemaJson() instead");

            if (!first)
                out += ",";
            first = false;

            out += "\"";
            out += std::string(name);
            out += "\":{\"type\":\"";
            out += Detail::PrimitiveTypeName<FieldT>();
            out += "\"}";

            required.emplace_back(std::string(name));
        }

        out += "},\"required\":[";
        for (size_t i = 0; i < required.size(); ++i)
        {
            if (i > 0)
                out += ",";
            out += "\"";
            out += required[i];
            out += "\"";
        }
        out += "]}";

        return out;
    }

    // Emits a schema for `T` and registers it under its type name.
    template <typename T>
    std::string EmitAndRegister(SchemaRegistry& reg)
    {
        std::string schema = EmitStructSchema<T>();
        std::string name { std::meta::identifier_of(^^T) };
        reg.Register(name, schema);
        return schema;
    }
} // namespace Baldr::OpenApi
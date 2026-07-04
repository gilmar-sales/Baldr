#include "JsonSchemaEmitter.hpp"

#include <cstdint>

bool SchemaRegistry::Register(std::string_view typeName, std::string schema)
{
    auto [it, inserted] =
        mSchemas.emplace(std::string(typeName), std::move(schema));
    return inserted;
}

bool SchemaRegistry::Contains(std::string_view typeName) const
{
    return mSchemas.contains(std::string(typeName));
}

std::string SchemaRegistry::RenderComponents() const
{
    std::string out   = "{";
    bool        first = true;
    for (const auto& [name, schema] : mSchemas)
    {
        if (!first)
            out += ",";
        first = false;
        out += "\"";
        out += name;
        out += "\":";
        out += schema;
    }
    out += "}";
    return out;
}

namespace Detail
{
    template <>
    const char* PrimitiveTypeName<std::string>()
    {
        return "string";
    }
    template <>
    const char* PrimitiveTypeName<std::string_view>()
    {
        return "string";
    }
    template <>
    const char* PrimitiveTypeName<int8_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<uint8_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<int16_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<uint16_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<int32_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<uint32_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<int64_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<uint64_t>()
    {
        return "integer";
    }
    template <>
    const char* PrimitiveTypeName<double>()
    {
        return "number";
    }
    template <>
    const char* PrimitiveTypeName<float>()
    {
        return "number";
    }
    template <>
    const char* PrimitiveTypeName<bool>()
    {
        return "boolean";
    }
} // namespace Detail
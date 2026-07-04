#pragma once

#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

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

    [[nodiscard]] const std::unordered_map<std::string, std::string>& Schemas()
        const
    {
        return mSchemas;
    }

  private:
    std::unordered_map<std::string, std::string> mSchemas;
};

namespace Detail
{
    template <typename T>
    const char* PrimitiveTypeName();

    template <typename T>
    struct IsSupportedField
        : std::bool_constant<
              std::is_same_v<T, std::string> ||
              std::is_same_v<T, std::string_view> || std::is_integral_v<T> ||
              std::is_same_v<T, double> || std::is_same_v<T, float> ||
              std::is_same_v<T, bool>>
    {
    };
} // namespace Detail

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
                  std::define_static_array(std::meta::nonstatic_data_members_of(
                      ^^T,
                      std::meta::access_context::current())))
    {
        constexpr auto name = std::meta::identifier_of(member);
        T              obj {};
        using FieldT = std::remove_cvref_t<decltype(obj.[:member:])>;

        static_assert(Detail::IsSupportedField<FieldT>::value,
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

template <typename T>
std::string EmitAndRegister(SchemaRegistry& reg)
{
    std::string schema = EmitStructSchema<T>();
    std::string name { std::meta::identifier_of(^^T) };
    reg.Register(name, schema);
    return schema;
}

namespace Detail
{
    template <typename T>
    constexpr bool HasMembersImpl()
    {
        if constexpr (!std::is_class_v<T>)
        {
            return false;
        }
        else
        {
            bool any = false;
            template for (constexpr auto member : std::define_static_array(
                              std::meta::nonstatic_data_members_of(
                                  ^^T,
                                  std::meta::access_context::current())))
            {
                (void) member;
                any = true;
            }
            return any;
        }
    }

    template <typename T>
    constexpr bool AllMembersSupportedImpl()
    {
        if constexpr (!std::is_class_v<T>)
        {
            return false;
        }
        else
        {
            bool ok   = true;
            bool seen = false;
            template for (constexpr auto member : std::define_static_array(
                              std::meta::nonstatic_data_members_of(
                                  ^^T,
                                  std::meta::access_context::current())))
            {
                seen = true;
                T obj {};
                using FieldT = std::remove_cvref_t<decltype(obj.[:member:])>;
                if constexpr (!IsSupportedField<FieldT>::value)
                    ok = false;
            }
            return seen && ok;
        }
    }
} // namespace Detail

// True when `T` is a class type that has at least one non-static data
// member and all of those members are in the supported primitive set.
// The "at least one" guard prevents empty standard containers and
// string-like types from being treated as reflectable structs.
template <typename T>
constexpr bool IsReflectableStruct =
    Detail::HasMembersImpl<T>() && Detail::AllMembersSupportedImpl<T>();

// True when `T` is auto-derivable: same as `IsReflectableStruct<T>`.
// Alias kept for readability at call sites. IResult / IStreamingResult
// subtypes are intentionally excluded by `IsReflectableStruct` because
// their JSON wire shape is decided inside their `Apply` / write
// method rather than encoded in the type's data members.
template <typename T>
constexpr bool IsAutoDerivable = IsReflectableStruct<T>;

// Emits and registers the schema for `T`, then returns a
// `{"$ref":"#/components/schemas/<name>"}` fragment. Returns
// `std::nullopt` for types that aren't auto-derivable. SFINAE keeps
// the non-derivable case compilable without forcing callers to
// branch on `IsAutoDerivable<T>` first.
namespace Detail
{
    template <typename T>
    struct IsVectorOfAutoDerivable : std::false_type
    {
    };

    template <typename U>
    struct IsVectorOfAutoDerivable<std::vector<U>>
        : std::bool_constant<IsAutoDerivable<U>>
    {
    };

    template <typename T>
    constexpr bool IsVectorOfAutoDerivableV =
        IsVectorOfAutoDerivable<std::remove_cvref_t<T>>::value;

    template <typename T>
    struct VectorElement;

    template <typename U>
    struct VectorElement<std::vector<U>>
    {
        using type = U;
    };

    template <typename U>
    std::string EmitArrayRefFor(SchemaRegistry& reg)
    {
        EmitAndRegister<U>(reg);
        std::string name { std::meta::identifier_of(^^U) };

        std::string out;
        out.reserve(name.size() + 48);
        out += "{\"type\":\"array\",\"items\":";
        out += "{\"$ref\":\"#/components/schemas/";
        out.append(name);
        out += "\"}}";
        return out;
    }
} // namespace Detail

template <typename T>
std::enable_if_t<IsAutoDerivable<T> && !Detail::IsVectorOfAutoDerivableV<T>,
                 std::optional<std::string>>
TryEmitRefFor(SchemaRegistry& reg)
{
    EmitAndRegister<T>(reg);
    std::string name { std::meta::identifier_of(^^T) };
    std::string out;
    out.reserve(name.size() + 32);
    out += "{\"$ref\":\"#/components/schemas/";
    out.append(name);
    out += "\"}";
    return out;
}

template <typename T>
std::enable_if_t<!IsAutoDerivable<T> && !Detail::IsVectorOfAutoDerivableV<T>,
                 std::optional<std::string>>
TryEmitRefFor(SchemaRegistry&)
{
    return std::nullopt;
}

template <typename T>
std::enable_if_t<Detail::IsVectorOfAutoDerivableV<T>,
                 std::optional<std::string>>
TryEmitRefFor(SchemaRegistry& reg)
{
    using Element =
        typename Detail::VectorElement<std::remove_cvref_t<T>>::type;
    return Detail::EmitArrayRefFor<Element>(reg);
}

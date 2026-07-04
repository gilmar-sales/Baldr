/**
 * @file OpenApi/JsonSchemaEmitter.hpp
 * @brief Compile-time JSON Schema emission from reflectable C++ types,
 *        plus a registry that deduplicates definitions across routes.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Deduplicating registry of JSON Schema definitions.
     *
     * Two routes using the same @c T share one entry under
     * @c components.schemas/<name> and reference it via @c $ref.
     */
    class SchemaRegistry
    {
      public:
        /**
         * @brief Insert or look up a schema.
         *
         * @param typeName Identifier (typically @c std::meta::identifier_of).
         * @param schema   JSON Schema fragment.
         * @return @c true if the type was newly registered; @c false if
         *         it was already present (and the existing schema string
         *         is unchanged).
         */
        bool Register(std::string_view typeName, std::string schema);

        /// @return @c true when @p typeName has been registered.
        [[nodiscard]] bool Contains(std::string_view typeName) const;

        /// @brief Render the contents of @c components.schemas as a JSON
        /// object.
        [[nodiscard]] std::string RenderComponents() const;

        /// @return Read-only view of every registered schema.
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
        /// @brief JSON Schema primitive type name for a supported field type.
        template <typename T>
        const char* PrimitiveTypeName();

        /**
         * @brief Trait that is @c true when @c T can be emitted as a
         *        JSON Schema primitive field by @ref EmitStructSchema.
         */
        template <typename T>
        struct IsSupportedField
            : std::bool_constant<
                  std::is_same_v<T, std::string> ||
                  std::is_same_v<T, std::string_view> ||
                  std::is_integral_v<T> || std::is_same_v<T, double> ||
                  std::is_same_v<T, float> || std::is_same_v<T, bool>>
        {
        };
    } // namespace Detail

    /**
     * @brief Emit a JSON Schema (draft-07) fragment describing @c T's
     *        non-static data members.
     *
     * Every member must satisfy @ref Detail::IsSupportedField. The
     * schema marks every member as @c required.
     *
     * @tparam T A class type with reflectable non-static data members.
     */
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
        template for (constexpr auto member : std::define_static_array(
                          std::meta::nonstatic_data_members_of(
                              ^^T,
                              std::meta::access_context::current())))
        {
            constexpr auto name = std::meta::identifier_of(member);
            T              obj {};
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

    /**
     * @brief Emit @c T's schema and register it in @p reg under its
     *        type identifier. Returns the schema string.
     */
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
                    using FieldT =
                        std::remove_cvref_t<decltype(obj.[:member:])>;
                    if constexpr (!IsSupportedField<FieldT>::value)
                        ok = false;
                }
                return seen && ok;
            }
        }
    } // namespace Detail

    /**
     * @brief @c true when @c T is a class type with at least one
     *        non-static data member and all members are in the supported
     *        primitive set.
     *
     * The "at least one" guard prevents empty standard containers and
     * string-like types from being treated as reflectable structs.
     */
    template <typename T>
    constexpr bool IsReflectableStruct =
        Detail::HasMembersImpl<T>() && Detail::AllMembersSupportedImpl<T>();

    /**
     * @brief @c true when @c T is auto-derivable. Alias for
     *        @ref IsReflectableStruct kept for readability at call sites.
     *
     * @c IResult / @c IStreamingResult subtypes are intentionally excluded
     * because their JSON wire shape is decided inside their @c Apply /
     * write method rather than encoded in the type's data members.
     */
    template <typename T>
    constexpr bool IsAutoDerivable = IsReflectableStruct<T>;

    /**
     * @brief Emit and register @c T's schema, then return a
     *        @c {"$ref":"#/components/schemas/<name>"} fragment.
     *
     * Returns @c std::nullopt for types that aren't auto-derivable.
     * SFINAE keeps the non-derivable case compilable without forcing
     * callers to branch on @ref IsAutoDerivable first.
     */
    namespace Detail
    {
        /// @brief Trait that is @c true when @c T is a @c std::vector
        ///        of an auto-derivable element type.
        template <typename T>
        struct IsVectorOfAutoDerivable : std::false_type
        {
        };

        template <typename U>
        struct IsVectorOfAutoDerivable<std::vector<U>>
            : std::bool_constant<IsAutoDerivable<U>>
        {
        };

        /// @brief Convenience alias for @ref IsVectorOfAutoDerivable.
        template <typename T>
        constexpr bool IsVectorOfAutoDerivableV =
            IsVectorOfAutoDerivable<std::remove_cvref_t<T>>::value;

        /// @brief Helper that exposes the element type of a @c std::vector.
        template <typename T>
        struct VectorElement;

        template <typename U>
        struct VectorElement<std::vector<U>>
        {
            using type = U; ///< Vector element type.
        };

        /**
         * @brief Emit an @c {"type":"array","items":{"$ref":...}}
         *        schema for the element type @c U and register @c U.
         */
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

    /**
     * @brief Emit a @c $ref fragment for an auto-derivable struct.
     */
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

    /**
     * @brief SFINAE fallback for non-derivable types. Returns
     *        @c std::nullopt; callers handle the absent schema.
     */
    template <typename T>
    std::enable_if_t<!IsAutoDerivable<T> &&
                         !Detail::IsVectorOfAutoDerivableV<T>,
                     std::optional<std::string>>
    TryEmitRefFor(SchemaRegistry&)
    {
        return std::nullopt;
    }

    /**
     * @brief Emit an array-of-ref fragment for a @c std::vector of an
     *        auto-derivable element type.
     */
    template <typename T>
    std::enable_if_t<Detail::IsVectorOfAutoDerivableV<T>,
                     std::optional<std::string>>
    TryEmitRefFor(SchemaRegistry& reg)
    {
        using Element =
            typename Detail::VectorElement<std::remove_cvref_t<T>>::type;
        return Detail::EmitArrayRefFor<Element>(reg);
    }

} // namespace BALDR_NAMESPACE

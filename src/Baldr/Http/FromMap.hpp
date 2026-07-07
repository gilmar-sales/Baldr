/**
 * @file Http/FromMap.hpp
 * @brief Internal reflection-driven binder shared by @c FromQuery<T> and
 *        @c FromParams<T>.
 *
 * Not part of the public API; the user-facing wrappers forward to
 * @ref baldr::detail::bindFromMap. The two sources differ only in which
 * @c HttpRequest map is consulted and in the error label.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{
    namespace detail
    {
        /// @brief Detects @c std::optional<U>.
        template <typename T>
        struct is_optional : std::false_type
        {
        };

        template <typename U>
        struct is_optional<std::optional<U>> : std::true_type
        {
        };

        /// @brief Convenience alias for @ref is_optional.
        template <typename T>
        inline constexpr bool is_optional_v = is_optional<T>::value;
        /**
         * @brief Value source used by @ref bindFromMap. Each
         *        implementation looks up @p key in the relevant
         *        @c HttpRequest map and exposes a human-readable @c label
         *        for error messages.
         */
        struct QuerySource
        {
            /**
             * @brief Look up @p key in @c HttpRequest::query.
             * @return The decoded value when present; @c std::nullopt
             * otherwise.
             */
            static std::optional<std::string_view> lookup(const HttpRequest& r,
                                                          std::string_view   k)
            {
                auto it = r.query.find(std::string(k));
                if (it == r.query.end())
                    return std::nullopt;
                return std::optional<std::string_view>(it->second);
            }

            /// @brief Short label used in bind error messages ("query").
            static constexpr std::string_view label() { return "query"; }
        };

        /**
         * @brief Value source used by @ref bindFromMap reading from
         *        @c HttpRequest::params.
         */
        struct ParamSource
        {
            /**
             * @brief Look up @p key in @c HttpRequest::params.
             * @return The decoded value when present; @c std::nullopt
             * otherwise.
             */
            static std::optional<std::string_view> lookup(const HttpRequest& r,
                                                          std::string_view   k)
            {
                auto it = r.params.find(std::string(k));
                if (it == r.params.end())
                    return std::nullopt;
                return std::optional<std::string_view>(it->second);
            }

            /// @brief Short label used in bind error messages ("path
            /// template").
            static constexpr std::string_view label()
            {
                return "path template";
            }
        };

        /**
         * @brief Parse a single string value into @p out, returning
         *        @c true on success.
         *
         * Strings and @c string_view are stored verbatim; the lifetime of
         * @c string_view values is owned by the underlying
         * @c HttpRequest::query / @c HttpRequest::params map, which
         * outlives the handler call. Numeric types use @c std::stod on the
         * input. Booleans accept @c "true" / @c "false" / @c "1" / @c "0".
         */
        template <typename T>
        bool parseMapField(std::string_view raw, T& out)
        {
            using FT = std::remove_cvref_t<T>;
            if constexpr (is_optional_v<FT>)
            {
                using Inner = typename FT::value_type;
                Inner tmp {};
                if (!parseMapField<Inner>(raw, tmp))
                    return false;
                out = std::move(tmp);
                return true;
            }
            else if constexpr (std::is_same_v<FT, std::string>)
            {
                out.assign(raw);
                return true;
            }
            else if constexpr (std::is_same_v<FT, std::string_view>)
            {
                out = raw;
                return true;
            }
            else if constexpr (std::is_same_v<FT, bool>)
            {
                if (raw == "true" || raw == "1")
                {
                    out = true;
                    return true;
                }
                if (raw == "false" || raw == "0")
                {
                    out = false;
                    return true;
                }
                return false;
            }
            else if constexpr (std::is_integral_v<FT> ||
                               std::is_same_v<FT, double> ||
                               std::is_same_v<FT, float>)
            {
                if (raw.empty())
                    return false;
                try
                {
                    std::string s(raw);
                    size_t      pos = 0;
                    if constexpr (std::is_integral_v<FT>)
                    {
                        long long v = std::stoll(s, &pos);
                        if (pos != s.size())
                            return false;
                        out = static_cast<FT>(v);
                    }
                    else
                    {
                        double v = std::stod(s, &pos);
                        if (pos != s.size())
                            return false;
                        out = static_cast<FT>(v);
                    }
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            else
            {
                static_assert(sizeof(FT) == 0,
                              "bindFromMap: unsupported field type");
                return false;
            }
        }

        /**
         * @brief Reflectable-field walker binding @c T from a request map.
         *
         * Strict semantics: every non-static data member of @c T must
         * have an entry in the source map and parse cleanly; otherwise
         * @c out.error is populated with status @c BadRequest and a message
         * naming the offending field.
         *
         * @tparam T      Reflectable payload type (members must be in the
         *                supported primitive set).
         * @tparam Source Either @c QuerySource or @c ParamSource.
         * @param request Incoming request.
         * @param out     Wrapper shell (e.g. @c FromQuery<T>) populated by
         *                this call. The wrapper's @c Error type must be
         *                constructible from
         *                @c {StatusCode, std::string}.
         */
        template <typename T, typename Source, typename Wrapper>
        void bindFromMap(const HttpRequest& request, Wrapper& out)
        {
            static_assert(std::is_class_v<T>,
                          "bindFromMap: T must be a class type");

            T     instance {};
            auto& value          = out.value;
            using ErrorT         = typename Wrapper::BindError;
            bool        anyError = false;
            std::string firstError;

            template for (constexpr auto member : std::define_static_array(
                              std::meta::nonstatic_data_members_of(
                                  ^^T,
                                  std::meta::access_context::current())))
            {
                if (anyError)
                    break;
                constexpr auto name = std::meta::identifier_of(member);
                auto&          f    = instance.[:member:];
                using FieldT        = std::remove_cvref_t<decltype(f)>;

                auto raw = Source::lookup(request, name);
                if (!raw.has_value())
                {
                    if constexpr (is_optional_v<FieldT>)
                    {
                        f = std::nullopt;
                        continue;
                    }
                    firstError =
                        "Field '" + std::string(name) + "' not present in " +
                        std::string(Source::label());
                    anyError = true;
                    continue;
                }

                if (!parseMapField<FieldT>(*raw, f))
                {
                    firstError = "Field '" + std::string(name) +
                                 "' could not be parsed as " +
                                 std::string(Source::label()) + " value";
                    anyError   = true;
                    continue;
                }
            }

            if (anyError)
            {
                out.error =
                    ErrorT { StatusCode::BadRequest, std::move(firstError) };
                return;
            }
            value = std::move(instance);
        }

    } // namespace detail
} // namespace BALDR_NAMESPACE

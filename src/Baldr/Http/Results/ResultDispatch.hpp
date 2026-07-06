/**
 * @file Http/Results/ResultDispatch.hpp
 * @brief Shared helper that applies a handler's return value to an
 *        @ref HttpResponse. Unwraps @c std::variant<...> alternatives by
 *        re-entering the same dispatch on the held value.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include <simdjson/simdjson.h>

#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/Results/StreamingResult.hpp>

namespace BALDR_NAMESPACE
{
    namespace detail
    {

        /// @brief @c true when @c T is a @c std::variant specialisation.
        template <typename T>
        struct IsStdVariant : std::false_type
        {
        };

        /// @brief @c std::variant specialisation of @ref IsStdVariant.
        template <typename... Ts>
        struct IsStdVariant<std::variant<Ts...>> : std::true_type
        {
        };

        /// @brief Convenience alias for @ref IsStdVariant.
        template <typename T>
        constexpr bool is_std_variant_v =
            IsStdVariant<std::remove_cvref_t<T>>::value;

        /**
         * @brief Trait selecting one of the supported dispatch paths for a
         *        given result type.
         *
         * The variants are evaluated top-down by @ref ApplyHandlerResult.
         * The streaming-in-variant branch is reached only when @c T is an
         * alternative inside a @c std::variant, and produces a clear
         * compile-time error.
         */
        template <typename T>
        constexpr bool IsStreamingVariantAlternative_v =
            std::is_base_of_v<IStreamingResult, std::remove_cvref_t<T>> &&
            !std::is_base_of_v<IResult, std::remove_cvref_t<T>>;

        /**
         * @brief Apply @p result to @p response using the framework's
         *        standard dispatch rules.
         *
         * Supported result shapes:
         *   - @c std::variant<...> — the active alternative is dispatched
         *     recursively through this function. Each alternative is
         *     handled exactly the way its concrete type dictates below.
         *     @c IStreamingResult alternatives inside a variant are
         *     rejected with a @c static_assert.
         *   - @c IStreamingResult (and only that) — stored in
         *     @c response.streaming for chunked delivery.
         *   - @c IResult — @c Apply is invoked.
         *   - @c const @c char* / @c char* — written as @c text/plain 200.
         *   - types assignable to @c std::string — written as @c text/plain
         * 200.
         *   - anything else — serialised to JSON with simdjson; on failure
         *     a @c text/plain 500 body is produced.
         *
         * @tparam T Result type. Deduced; the same overload accepts both
         *           concrete values and references.
         * @param result   The value returned by the handler.
         * @param response The response object to populate.
         */
        template <typename T>
        void ApplyHandlerResult(T&& result, HttpResponse& response)
        {
            using BareT = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<BareT, std::monostate>)
            {
                response.statusCode = StatusCode::OK;
            }
            else if constexpr (std::is_base_of_v<IStreamingResult, BareT> &&
                               !std::is_base_of_v<IResult, BareT>)
            {
                response.streaming =
                    std::make_shared<BareT>(std::forward<T>(result));
            }
            else if constexpr (std::is_base_of_v<IResult, BareT>)
            {
                result.Apply(response);
            }
            else if constexpr (std::is_same_v<const char*, BareT> ||
                               std::is_same_v<char*, BareT>)
            {
                response.headers["Content-Type"] = "text/plain";
                response.body                    = std::string(result);
                response.statusCode              = StatusCode::OK;
            }
            else if constexpr (std::is_assignable_v<std::string, BareT>)
            {
                response.headers["Content-Type"] = "text/plain";
                response.body                    = result;
                response.statusCode              = StatusCode::OK;
            }
            else if constexpr (is_std_variant_v<BareT>)
            {
                std::visit(
                    [&response](auto&& held) {
                        using HeldBare = std::remove_cvref_t<decltype(held)>;
                        static_assert(
                            !IsStreamingVariantAlternative_v<HeldBare>,
                            "Baldr handler variants cannot contain "
                            "IStreamingResult alternatives — streaming "
                            "semantics assume a single owner of the "
                            "response stream.");
                        ApplyHandlerResult(std::forward<decltype(held)>(held),
                                           response);
                    },
                    std::forward<T>(result));
            }
            else
            {
                simdjson::simdjson_result<std::string> json =
                    simdjson::to_json(result);
                if (json.has_value())
                {
                    response.body = std::move(json).take_value();
                    response.headers["Content-Type"] = "application/json";
                    response.statusCode              = StatusCode::OK;
                }
                else
                {
                    response.headers["Content-Type"] = "text/plain";
                    response.body = "Handler returned a value that could not "
                                    "be "
                                    "serialized to JSON or std::string.";
                    response.statusCode = StatusCode::InternalServerError;
                }
            }
        }

    } // namespace detail

} // namespace BALDR_NAMESPACE
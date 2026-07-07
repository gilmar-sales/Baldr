/**
 * @file Http/FromParams.hpp
 * @brief @c FromParams<T> parameter-binding wrapper.
 *
 * Handlers declare a parameter of type @c FromParams<T> to opt into
 * automatic reflection-driven binding from the request's path-template
 * parameters (@c HttpRequest::params). The router resolves the parameter
 * before the handler runs and hands back a value shell that either holds
 * the parsed @c T or an error describing which field was missing or
 * failed to parse.
 *
 * Constraints on @c T mirror @ref parseJson: every non-static data
 * member must be one of @c std::string, @c std::string_view, integral,
 * @c double, @c float, or @c bool. The same strict-by-default contract
 * as @c FromBody<T> applies.
 *
 * @code
 * app->MapGet("/users/:id", [](baldr::FromParams<UserPathArgs> p) -> IResult {
 *     if (!p.isOk())
 *         return JsonResult(p.error->statusCode,
 *                           JsonBody{ .message = p.error->message });
 *     // use p.Value().id
 * });
 * @endcode
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <Baldr/Http/FromMap.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Marker wrapper telling the router "bind this parameter from
     *        the request's path-template captures".
     *
     * The wrapper is passed by value to the handler; @c value is the
     * parsed struct (default-constructed on failure) and @c error carries
     * the failure details when present.
     *
     * @tparam T A reflectable struct whose members are all in the
     *           supported primitive set (see @ref parseJson).
     */
    template <typename T>
    class FromParams
    {
      public:
        /// @brief The deserialised payload type.
        using ValueType = T;

        /// @brief Error payload when binding fails.
        struct BindError
        {
            StatusCode  statusCode = StatusCode::OK; ///< HTTP status.
            std::string message;                     ///< Diagnostic message.
        };

        /// @brief The parsed object on success; default-constructed otherwise.
        T value {};

        /// @brief Populated when binding failed; @c std::nullopt on success.
        std::optional<BindError> error {};

        /// @return @c true when binding succeeded.
        bool isOk() const { return !error.has_value(); }

        /// @return Read-only access to the parsed value.
        const T& Value() const { return value; }
        /// @return Mutable access to the parsed value.
        T& Value() { return value; }

        /// @return The bind error, or @c std::nullopt on success.
        std::optional<BindError> getError() const { return error; }
    };

    /// @brief Trait detecting whether a type is an instantiation of @ref
    /// FromParams.
    template <typename U>
    struct isFromParams : std::false_type
    {
    };

    /// @brief Specialisation for valid @ref FromParams instantiations.
    template <typename U>
    struct isFromParams<FromParams<U>> : std::true_type
    {
        /// @brief The wrapped payload type.
        using ValueType = U;
    };

    /// @brief Convenience alias for @ref isFromParams.
    template <typename U>
    inline constexpr bool isFromParams_v = isFromParams<U>::value;

    namespace detail
    {
        /**
         * @brief Bind @c FromParams<T> from @p request.
         *
         * @tparam T Reflectable payload type.
         * @param request Incoming request.
         * @return A populated @ref FromParams<T> shell.
         */
        template <typename T>
        FromParams<T> bindFromParams(const HttpRequest& request)
        {
            FromParams<T> out;
            bindFromMap<T, ParamSource, FromParams<T>>(request, out);
            return out;
        }
    } // namespace detail
} // namespace BALDR_NAMESPACE

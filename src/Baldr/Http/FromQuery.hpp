/**
 * @file Http/FromQuery.hpp
 * @brief @c FromQuery<T> parameter-binding wrapper.
 *
 * Handlers declare a parameter of type @c FromQuery<T> to opt into
 * automatic reflection-driven binding from the request's query string
 * (@c HttpRequest::query). The router resolves the parameter before the
 * handler runs and hands back a value shell that either holds the parsed
 * @c T or an error describing which field was missing or failed to
 * parse.
 *
 * Constraints on @c T mirror @ref parseJson: every non-static data
 * member must be one of @c std::string, @c std::string_view, integral,
 * @c double, @c float, or @c bool. The same strict-by-default contract
 * as @c FromBody<T> applies.
 *
 * @code
 * app->MapGet("/search", [](baldr::FromQuery<SearchFilters> q) -> IResult {
 *     if (!q.isOk())
 *         return JsonResult(q.error->statusCode,
 *                           JsonBody{ .message = q.error->message });
 *     // use q.value.name / .age
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
     *        the request's query string".
     *
     * The wrapper is passed by value to the handler; @c value is the
     * parsed struct (default-constructed on failure) and @c error carries
     * the failure details when present.
     *
     * @tparam T A reflectable struct whose members are all in the
     *           supported primitive set (see @ref parseJson).
     */
    template <typename T>
    class FromQuery
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

        /// @return The bind error, or @c std::nullopt on success.
        std::optional<BindError> getError() const { return error; }
    };

    /// @brief Trait detecting whether a type is an instantiation of @ref
    /// FromQuery.
    template <typename U>
    struct isFromQuery : std::false_type
    {
    };

    /// @brief Specialisation for valid @ref FromQuery instantiations.
    template <typename U>
    struct isFromQuery<FromQuery<U>> : std::true_type
    {
        /// @brief The wrapped payload type.
        using ValueType = U;
    };

    /// @brief Convenience alias for @ref isFromQuery.
    template <typename U>
    inline constexpr bool isFromQuery_v = isFromQuery<U>::value;

    namespace detail
    {
        /**
         * @brief Bind @c FromQuery<T> from @p request.
         *
         * @tparam T Reflectable payload type.
         * @param request Incoming request.
         * @return A populated @ref FromQuery<T> shell.
         */
        template <typename T>
        FromQuery<T> bindFromQuery(const HttpRequest& request)
        {
            FromQuery<T> out;
            bindFromMap<T, QuerySource, FromQuery<T>>(request, out);
            return out;
        }
    } // namespace detail
} // namespace BALDR_NAMESPACE

/**
 * @file Http/FromBody.hpp
 * @brief @c FromBody<T> parameter-binding wrapper.
 *
 * Handlers declare a parameter of type @c FromBody<T> to opt into automatic
 * JSON body deserialisation. The router resolves the parameter before the
 * handler runs and hands back a value shell that either holds the parsed
 * @c T or an error describing why the bind failed.
 *
 * When binding fails the router short-circuits the handler and writes a
 * structured error response itself (a 400 @c application/json
 * @c {"field":"<name>","message":"<reason>"} body, or the underlying
 * status code with a plain-text body for non-400 errors such as
 * @c 415 UnsupportedMediaType). Handlers therefore only run when the
 * body is parseable; they can still inspect @c error if they want to
 * react to a partial parse.
 *
 * The class does not allocate outside what @c T itself stores. Bind failure
 * is reported in-band (no exceptions).
 *
 * @code
 * app->MapPost("/login", [](baldr::FromBody<UserLoginDto> login) -> IResult {
 *     // login.isOk() is guaranteed true when the handler runs.
 *     // use login.value.username / .password
 * });
 * @endcode
 *
 * Constraints on @c T are inherited from @ref baldr::parseJson: every
 * non-static data member must be one of @c std::string, @c std::string_view,
 * integral, @c double, @c float, or @c bool, or a specialisation of
 * @c BALDR_NAMESPACE::detail::readJsonField must exist for it.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <Baldr/Http/FromParams.hpp>
#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/JsonBody.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Marker wrapper telling the router "bind this parameter from the
     *        HTTP request body".
     *
     * The router deserialises @c HttpRequest::body as a JSON object into
     * @c T using @ref parseJson. The wrapper is passed by value to the
     * handler; @c value is the parsed struct (default-constructed on
     * failure) and @c error carries the failure details when present.
     *
     * @tparam T A reflectable struct satisfying the @ref parseJson
     *           constraints.
     */
    template <typename T>
    class FromBody
    {
      public:
        /// @brief The deserialised payload type.
        using ValueType = T;

        /// @brief The parsed object on success; default-constructed otherwise.
        T value {};

        /// @brief Populated when binding failed; @c std::nullopt on success.
        std::optional<typename JsonBodyResult<T>::Error> error {};

        /// @return @c true when binding succeeded.
        bool isOk() const { return !error.has_value(); }

        /// @return The bind error, or @c std::nullopt on success.
        std::optional<typename JsonBodyResult<T>::Error> Error() const
        {
            return error;
        }
    };

    /// @brief Trait detecting whether a type is an instantiation of @ref
    /// FromBody.
    template <typename U>
    struct isFromBody : std::false_type
    {
    };

    /// @brief Specialisation for valid @ref FromBody instantiations.
    template <typename U>
    struct isFromBody<FromBody<U>> : std::true_type
    {
        /// @brief The wrapped payload type.
        using ValueType = U;
    };

    /// @brief Convenience alias for @ref isFromBody.
    template <typename U>
    inline constexpr bool isFromBody_v = isFromBody<U>::value;

    namespace detail
    {
        /**
         * @brief Case-insensitive search for @p needle in @p haystack.
         *
         * @param haystack String to inspect.
         * @param needle   ASCII substring to look for.
         * @return @c true when @p needle is found (case-insensitively).
         */
        inline bool containsCi(std::string_view haystack,
                               std::string_view needle)
        {
            if (needle.size() > haystack.size())
                return false;
            for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i)
            {
                bool match = true;
                for (std::size_t j = 0; j < needle.size(); ++j)
                {
                    char a = haystack[i + j];
                    char b = needle[j];
                    if (a >= 'A' && a <= 'Z')
                        a = static_cast<char>(a + 32);
                    if (b >= 'A' && b <= 'Z')
                        b = static_cast<char>(b + 32);
                    if (a != b)
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
            return false;
        }

        /**
         * @brief Parse @p request's body as JSON and wrap the result in a
         *        @ref FromBody<T>.
         *
         * Behaviour:
         * - Empty body, malformed JSON, top-level non-object, or missing
         *   field → failure with @c StatusCode::BadRequest.
         * - @c Content-Type header present and not @c application/json →
         *   failure with @c StatusCode::UnsupportedMediaType.
         * - Missing @c Content-Type header → permissive; still attempts the
         *   parse.
         *
         * @tparam T Reflectable payload type (see @ref parseJson).
         * @param request Incoming request.
         * @return A populated @ref FromBody<T> shell.
         */
        template <typename T>
        FromBody<T> bindFromBody(const HttpRequest& request)
        {
            FromBody<T> out;

            if (auto it = request.headers.find("content-type");
                it != request.headers.end() && !it->second.empty())
            {
                if (!containsCi(it->second, "application/json"))
                {
                    typename JsonBodyResult<T>::Error err {
                        StatusCode::UnsupportedMediaType,
                        "FromBody<T> requires Content-Type: application/json"
                    };
                    out.error = err;
                    return out;
                }
            }

            auto r = parseJson<T>(request);
            if (r.isOk())
            {
                out.value = r.takeValue();
            }
            else
            {
                out.error = r.error();
            }
            return out;
        }

        /// @brief Empty placeholder slot in @c boundBodies for non-wrapper
        /// parameters. Never read.
        struct EmptySlot
        {
        };

        /// @brief @c true when @c T is any of the pre-bound wrappers
        /// (@c FromBody<T>, @c FromQuery<T>, @c FromParams<T>).
        template <typename T>
        struct IsBoundWrapper
            : std::bool_constant<isFromBody_v<T> || isFromQuery_v<T> ||
                                 isFromParams_v<T>>
        {
        };

        /// @brief Convenience alias for @ref IsBoundWrapper.
        template <typename T>
        inline constexpr bool IsBoundWrapper_v = IsBoundWrapper<T>::value;

        /// @brief Build the pre-bound tuple type for a handler argument
        /// list.
        ///
        /// For each handler parameter index @c I:
        /// - If the parameter is any of the pre-bound wrappers, the slot
        ///   holds that wrapper's value type (default-constructed and
        ///   populated by the appropriate @c bindFrom* call).
        /// - Otherwise the slot holds @c EmptySlot (never read).
        template <typename HandlerArgsTuple>
        struct BuildBoundBodies;

        template <typename... Args>
        struct BuildBoundBodies<std::tuple<Args...>>
        {
            using type =
                std::tuple<std::conditional_t<IsBoundWrapper_v<Args>,
                                              std::remove_cvref_t<Args>,
                                              EmptySlot>...>;
        };

        /// @brief Bind slot @c I of @p boundBodies from @p request when
        /// the corresponding handler parameter is a pre-bound wrapper.
        ///
        /// Dispatches on @c isFromBody_v / @c isFromQuery_v /
        /// @c isFromParams_v to call the correct binder.
        template <std::size_t I,
                  typename HandlerArgsTuple,
                  typename BoundBodiesTuple>
        inline void BindOneBodySlot(BoundBodiesTuple&  boundBodies,
                                    const HttpRequest& request)
        {
            using Arg = std::tuple_element_t<I, HandlerArgsTuple>;
            if constexpr (isFromBody_v<Arg>)
            {
                using Payload            = typename Arg::ValueType;
                std::get<I>(boundBodies) = bindFromBody<Payload>(request);
            }
            else if constexpr (isFromQuery_v<Arg>)
            {
                using Payload            = typename Arg::ValueType;
                std::get<I>(boundBodies) = bindFromQuery<Payload>(request);
            }
            else if constexpr (isFromParams_v<Arg>)
            {
                using Payload            = typename Arg::ValueType;
                std::get<I>(boundBodies) = bindFromParams<Payload>(request);
            }
        }

        /// @brief Backwards-compatible alias for @ref BindOneBodySlot.
        template <std::size_t I,
                  typename HandlerArgsTuple,
                  typename BoundBodiesTuple>
        inline void BindOneBody(BoundBodiesTuple&  boundBodies,
                                const HttpRequest& request)
        {
            BindOneBodySlot<I, HandlerArgsTuple, BoundBodiesTuple>(boundBodies,
                                                                   request);
        }

        /// @brief Render a JSON body of the form
        ///        @c {"field":"<field>","message":"<message>"} with the
        ///        obvious characters escaped.
        inline std::string WriteBindErrorJson(const std::string& field,
                                              const std::string& message)
        {
            std::string out;
            out.reserve(field.size() + message.size() + 32);
            out.append(R"({"field":")");
            for (char c : field)
            {
                if (c == '"' || c == '\\')
                    out.push_back('\\');
                out.push_back(c);
            }
            out.append(R"(","message":")");
            for (char c : message)
            {
                if (c == '"' || c == '\\')
                    out.push_back('\\');
                if (c == '\n')
                {
                    out.push_back('\\');
                    out.push_back('n');
                    continue;
                }
                if (c == '\r')
                {
                    out.push_back('\\');
                    out.push_back('r');
                    continue;
                }
                if (c == '\t')
                {
                    out.push_back('\\');
                    out.push_back('t');
                    continue;
                }
                out.push_back(c);
            }
            out.append(R"("})");
            return out;
        }

        /// @brief Look at @p boundBodies; if any pre-bound slot failed,
        ///        write a structured error response to @p response and
        ///        return @c true. Otherwise return @c false.
        ///
        /// Used by the router to short-circuit a handler invocation when
        /// a @ref FromBody, @ref FromQuery, or @ref FromParams parameter
        /// could not be deserialised. The response body is the JSON
        /// @c {"field":"<name or empty>","message":"<error message>"}.
        template <typename BoundBodiesTuple>
        bool WriteBindErrorResponse(const BoundBodiesTuple& boundBodies,
                                    HttpResponse&           response)
        {
            bool handled = false;
            std::apply(
                [&](const auto&... slots) {
                    auto visit = [&](const auto& slot) {
                        using SlotT = std::remove_cvref_t<decltype(slot)>;
                        if constexpr (IsBoundWrapper_v<SlotT>)
                        {
                            if (slot.isOk())
                                return;
                            if (handled)
                                return;
                            handled = true;

                            StatusCode  status = StatusCode::BadRequest;
                            std::string field;
                            std::string message = "Request could not be bound";

                            if (slot.error)
                            {
                                status  = slot.error->statusCode;
                                message = slot.error->message;
                                using ErrorT =
                                    std::remove_cvref_t<decltype(*slot.error)>;
                                if constexpr (requires { ErrorT::field; })
                                {
                                    if (slot.error->field.has_value())
                                        field = *slot.error->field;
                                }
                            }

                            if (field.empty())
                            {
                                std::string prefix = "Field '";
                                auto        pos    = message.find(prefix);
                                if (pos == 0)
                                {
                                    auto end =
                                        message.find('\'', prefix.size());
                                    if (end != std::string::npos)
                                    {
                                        field =
                                            message.substr(prefix.size(),
                                                           end - prefix.size());
                                    }
                                }
                            }

                            response.statusCode = status;
                            if (status == StatusCode::BadRequest)
                            {
                                response.headers["Content-Type"] =
                                    "application/json";
                                response.body =
                                    WriteBindErrorJson(field, message);
                            }
                            else
                            {
                                response.headers["Content-Type"] = "text/plain";
                                response.body = std::move(message);
                            }
                        }
                    };
                    (visit(slots), ...);
                },
                boundBodies);
            return handled;
        }

        /// @brief Compile-time index of the first @c TWrapper in the
        /// handler tuple. Matches any of the pre-bound wrappers by
        /// exact @c remove_cvref_t equality. Fails to compile when no
        /// match is found.
        template <typename HandlerArgsTuple, typename TWrapper>
        struct IndexOfBoundBody;

        template <typename TWrapper, typename... Args>
        struct IndexOfBoundBody<std::tuple<Args...>, TWrapper>
        {
          private:
            template <typename A>
            static constexpr bool matches_v =
                std::is_same_v<std::remove_cvref_t<A>,
                               std::remove_cvref_t<TWrapper>> &&
                IsBoundWrapper_v<std::remove_cvref_t<A>>;

            template <std::size_t I>
            static constexpr std::size_t find()
            {
                if constexpr (I >= sizeof...(Args))
                {
                    static_assert(sizeof...(Args) == 0,
                                  "IndexOfBoundBody: TWrapper not found in "
                                  "HandlerArgsTuple");
                    return I;
                }
                else if constexpr (
                    matches_v<std::tuple_element_t<I, std::tuple<Args...>>>)
                {
                    return I;
                }
                else
                {
                    return find<I + 1>();
                }
            }

          public:
            static constexpr std::size_t value = find<0>();
        };

        /// @brief Backwards-compatible alias for @ref IndexOfBoundBody.
        template <typename HandlerArgsTuple, typename TFromBody>
        using IndexOfFromBody = IndexOfBoundBody<HandlerArgsTuple, TFromBody>;

        /// @brief Tag type carrying a handler argument type. Lets the caller
        /// pass @c HandlerArgsTuple elements to a generic factory without
        /// losing type information.
        template <typename T>
        struct ArgTag
        {
            using type = T;
        };

        /// @brief Build a @c std::tuple by invoking @p makeOne for each
        /// element of @c HandlerArgsTuple, passing a tag carrying the
        /// element type.
        ///
        /// The factory must return a value of exactly the element type
        /// (references included, e.g. @c HttpRequest&). Brace-initialisation
        /// of @c std::tuple<T&> is not used here because it does not work
        /// for reference types in libstdc++; call-syntax construction is.
        template <typename HandlerArgsTuple, typename Factory>
        auto BuildArgsTuple(Factory&& makeOne)
        {
            constexpr std::size_t N = std::tuple_size_v<HandlerArgsTuple>;
            return [&]<std::size_t... I>(std::index_sequence<I...>) {
                return std::tuple<std::tuple_element_t<I, HandlerArgsTuple>...>(
                    std::forward<Factory>(makeOne)(
                        ArgTag<
                            std::tuple_element_t<I, HandlerArgsTuple>> {})...);
            }(std::make_index_sequence<N> {});
        }
    } // namespace detail
} // namespace BALDR_NAMESPACE
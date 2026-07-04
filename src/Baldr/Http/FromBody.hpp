/**
 * @file Http/FromBody.hpp
 * @brief @c FromBody<T> parameter-binding wrapper.
 *
 * Handlers declare a parameter of type @c FromBody<T> to opt into automatic
 * JSON body deserialisation. The router resolves the parameter before the
 * handler runs and hands back a value shell that either holds the parsed
 * @c T or an error describing why the bind failed.
 *
 * The class does not allocate outside what @c T itself stores. Bind failure
 * is reported in-band (no exceptions) so handlers always run; the handler
 * is responsible for translating @c error into an HTTP response.
 *
 * @code
 * app->MapPost("/login", [](baldr::FromBody<UserLoginDto> login) -> IResult {
 *     if (!login.isOk())
 *         return JsonResult(login.error->statusCode,
 *                           JsonBody{ .message = login.error->message });
 *     // use login.Value().username / .password
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

#include <Baldr/Http/Request.hpp>
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

        /// @return Read-only access to the parsed value.
        const T& Value() const { return value; }

        /// @return Mutable access to the parsed value.
        T& Value() { return value; }

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

        /**
         * @brief Slot type for the pre-bound @ref FromBody tuple.
         *
         * For each handler parameter index @c I:
         * - If the parameter is @c FromBody<U>, the slot holds a
         *   @ref FromBody<U> instance populated by @ref bindFromBody.
         * - Otherwise the slot holds @c EmptySlot (never read).
         */
        template <typename Slot>
        struct BodySlot
        {
            Slot value;
        };

        /// @brief Empty placeholder slot in @c boundBodies for non-FromBody
        /// parameters. Never read.
        struct EmptySlot
        {
        };

        /// @brief Build the pre-bound tuple type for a handler argument list.
        template <typename HandlerArgsTuple>
        struct BuildBoundBodies;

        template <typename... Args>
        struct BuildBoundBodies<std::tuple<Args...>>
        {
            using type =
                std::tuple<std::conditional_t<isFromBody_v<Args>,
                                              std::remove_cvref_t<Args>,
                                              EmptySlot>...>;
        };

        /// @brief Bind slot @c I of @p boundBodies from @p request when the
        /// corresponding handler parameter is a @ref FromBody<T>.
        template <std::size_t I,
                  typename HandlerArgsTuple,
                  typename BoundBodiesTuple>
        inline void BindOneBody(BoundBodiesTuple&  boundBodies,
                                const HttpRequest& request)
        {
            using Arg = std::tuple_element_t<I, HandlerArgsTuple>;
            if constexpr (isFromBody_v<Arg>)
            {
                using Payload            = typename Arg::ValueType;
                std::get<I>(boundBodies) = bindFromBody<Payload>(request);
            }
        }

        /// @brief Compile-time index of the first @c FromBody<TFromBody> in
        /// the handler tuple. Fails to compile if there is no match.
        template <typename HandlerArgsTuple, typename TFromBody>
        struct IndexOfFromBody;

        template <typename TFromBody, typename... Args>
        struct IndexOfFromBody<std::tuple<Args...>, TFromBody>
        {
          private:
            template <typename A>
            static constexpr bool matches_v =
                std::is_same_v<std::remove_cvref_t<A>,
                               std::remove_cvref_t<TFromBody>> &&
                isFromBody_v<std::remove_cvref_t<A>>;

            template <std::size_t I>
            static constexpr std::size_t find()
            {
                if constexpr (I >= sizeof...(Args))
                {
                    static_assert(sizeof...(Args) == 0,
                                  "IndexOfFromBody: TFromBody not found in "
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
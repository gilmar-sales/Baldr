/**
 * @file Http/Router.hpp
 * @brief Path/method router that turns incoming requests into handler
 *        invocations and exposes its registered routes for introspection.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/FromParams.hpp>
#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/Tuple.hpp>

namespace BALDR_NAMESPACE
{

    class SchemaRegistry;

    /**
     * @brief Callable invoked once a route has been matched.
     *
     * Receives the request (mutable, so middleware may attach context),
     * the response (mutable, populated by the handler), and the
     * request-scoped service provider used to resolve handler
     * dependencies.
     */
    using RouteHandler = std::function<void(
        HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>)>;

    /**
     * @brief Internal record of a registered route.
     *
     * Produced by the router during @ref Router::insert and consumed by
     * the matcher and the OpenAPI spec service. The compiled regex
     * captures named path parameters in declaration order.
     */
    struct RouteEntry
    {
        std::regex
            extractParamsRegex; ///< Compiled regex for path-parameter capture.
        std::vector<std::string>
             paramsNames = {};   ///< Ordered names of captured path parameters.
        bool hasParams   = true; ///< Whether the template contains parameters.
        RouteHandler handler;    ///< Callable to invoke on a match.
        RouteOptions options;    ///< OpenAPI options for the route.
        std::string  groupPrefix; ///< Group prefix, if any.
        std::string pathTemplate; ///< Original template (e.g. @c "/users/:id").
        HttpMethod  method { HttpMethod::Get }; ///< HTTP method.

        /**
         * @brief Extract named path parameters from a concrete request path.
         *
         * @param path The request path to match against this entry's regex.
         * @return Map of parameter name to decoded value.
         */
        std::unordered_map<std::string, std::string> extractRouteParams(
            const std::string& path) const;
    };

    /**
     * @brief Path-and-method router with regex-based parameter capture.
     *
     * Routes are matched in registration order with first-match wins.
     * Use @ref matchWithAllow to obtain the full @c Allow header for
     * 405 responses, or @ref Snapshot to enumerate every registered
     * route (used by the OpenAPI extension).
     */
    class Router
    {
      public:
        Router();

        ~Router();
        Router(const Router&)                = delete;
        Router& operator=(const Router&)     = delete;
        Router(Router&&) noexcept            = default;
        Router& operator=(Router&&) noexcept = default;

        template <typename Handler>
        void MapRoute(HttpMethod method, const std::string& route,
                      const std::string&  groupPrefix,
                      const RouteOptions& options, Handler&& handler)
        {
            insert(
                method,
                route,
                options,
                groupPrefix,
                [handler = std::forward<Handler>(handler)](
                    HttpRequest&  request,
                    HttpResponse& response,
                    const skr::Arc<skr::ServiceProvider>&
                        serviceProvider) mutable {
                    using HandlerArgsTuple = typename LambdaTraits<
                        std::remove_reference_t<decltype(handler)>>::ArgsTuple;

                    constexpr std::size_t N =
                        std::tuple_size_v<HandlerArgsTuple>;

                    using BoundBodiesTuple = typename detail::BuildBoundBodies<
                        HandlerArgsTuple>::type;

                    BoundBodiesTuple boundBodies {};

                    [&]<std::size_t... I>(std::index_sequence<I...>) {
                        (detail::BindOneBodySlot<I, HandlerArgsTuple>(
                             boundBodies, request),
                         ...);
                    }(std::make_index_sequence<N> {});

                    auto args = detail::BuildArgsTuple<HandlerArgsTuple>(
                        [&](auto tag) -> typename decltype(tag)::type {
                            using TArg     = typename decltype(tag)::type;
                            using BareTArg = std::remove_cvref_t<TArg>;

                            if constexpr (std::is_same_v<BareTArg, HttpRequest>)
                            {
                                return request;
                            }
                            else if constexpr (std::is_same_v<BareTArg,
                                                              HttpResponse>)
                            {
                                return response;
                            }
                            else if constexpr (isFromBody_v<BareTArg> ||
                                               isFromQuery_v<BareTArg> ||
                                               isFromParams_v<BareTArg>)
                            {
                                constexpr std::size_t Idx =
                                    detail::IndexOfBoundBody<HandlerArgsTuple,
                                                             BareTArg>::value;
                                return std::get<Idx>(boundBodies);
                            }
                            else if constexpr (skr::is_arc_v<BareTArg>)
                            {
                                return serviceProvider->GetService<
                                    typename BareTArg::element_type>();
                            }
                            else
                            {
                                static_assert(
                                    sizeof(BareTArg*) == 0,
                                    "Unsupported handler argument type. "
                                    "Handler parameters must be one of: "
                                    "Baldr::HttpRequest, Baldr::HttpResponse, "
                                    "a FromBody/FromQuery/FromParams-bound "
                                    "type, or an skr::Arc<...> of a "
                                    "registered service.");
                            }
                        });

                    using ResultType = LambdaResult<decltype(handler)>;
                    if constexpr (!std::is_same_v<ResultType, void>)
                    {
                        auto result = std::apply(handler, args);

                        if constexpr (std::is_base_of_v<IStreamingResult,
                                                        ResultType> &&
                                      !std::is_base_of_v<IResult, ResultType>)
                        {
                            response.streaming =
                                std::make_shared<ResultType>(std::move(result));
                        }
                        else if constexpr (std::is_base_of_v<IResult,
                                                             ResultType>)
                        {
                            result.Apply(response);
                        }
                        else if constexpr (std::is_same_v<const char*,
                                                          ResultType> ||
                                           std::is_same_v<char*, ResultType>)
                        {
                            response.headers["Content-Type"] = "text/plain";
                            response.body       = std::string(result);
                            response.statusCode = StatusCode::OK;
                        }
                        else if constexpr (std::is_assignable_v<std::string,
                                                                ResultType>)
                        {
                            response.headers["Content-Type"] = "text/plain";
                            response.body                    = result;
                            response.statusCode              = StatusCode::OK;
                        }
                        else
                        {
                            simdjson::simdjson_result<std::string> json =
                                simdjson::to_json(result);
                            if (json.has_value())
                            {
                                response.body = std::move(json).take_value();
                                response.headers["Content-Type"] =
                                    "application/json";
                                response.statusCode = StatusCode::OK;
                            }
                            else
                            {
                                response.headers["Content-Type"] = "text/plain";
                                response.body =
                                    "Handler returned a value that could not "
                                    "be "
                                    "serialized to JSON or std::string.";
                                response.statusCode =
                                    StatusCode::InternalServerError;
                            }
                        }
                    }
                    else
                    {
                        std::apply(handler, args);
                    }
                });
        }

        /**
         * @brief Lower-level route insertion used by middleware-aware paths.
         *
         * Bypasses the per-request DI machinery; intended for internal use.
         */
        void insert(HttpMethod method, std::string path,
                    const RouteHandler& routeHandler) const;

        /**
         * @brief Insert a route with explicit OpenAPI options and group
         *        prefix. The primary registration entry point.
         */
        void insert(HttpMethod method, std::string path, RouteOptions options,
                    std::string         groupPrefix,
                    const RouteHandler& routeHandler) const;

        /**
         * @brief Find the first route matching @p method and @p path.
         *
         * @return The matched entry, or @c std::nullopt if no route matches.
         */
        [[nodiscard]] std::optional<RouteEntry> match(HttpMethod,
                                                      std::string path) const;

        /**
         * @brief Result of a match that also reports which methods are
         *        registered for the same path template.
         */
        struct MatchResult
        {
            std::optional<RouteEntry> entry; ///< Matched route, if any.
            std::vector<HttpMethod>
                allowedMethodsOnPath; ///< Methods registered at this path.
            HttpMethod resolvedMethod =
                HttpMethod::Get; ///< Echo of the requested method.
            std::string
                routeTemplate; ///< Template matched (when an entry exists).
        };

        /**
         * @brief Like @ref match but always returns the @c MatchResult so
         *        callers can emit a precise @c Allow header on 405.
         */
        [[nodiscard]] MatchResult matchWithAllow(HttpMethod  method,
                                                 std::string path) const;

        /**
         * @brief Snapshot of every registered route.
         *
         * Used by the OpenAPI spec service to render the spec document.
         */
        std::vector<RouteEntry> Snapshot() const;

        /**
         * @brief Access the per-router JSON Schema registry used by the
         *        OpenAPI extension to deduplicate schema definitions.
         */
        const skr::Arc<SchemaRegistry>& SchemaRegistrySlot() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace BALDR_NAMESPACE
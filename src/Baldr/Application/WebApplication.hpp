/**
 * @file Application/WebApplication.hpp
 * @brief Top-level application object that owns the HTTP router, registers
 *        routes and middleware, and drives the request loop.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Tuple.hpp>

#include "WebApplication_Impl.hpp"

namespace BALDR_NAMESPACE
{

    /**
     * @brief Baldr's user-facing application entry point.
     *
     * Wraps a Skirnir service container and exposes a fluent API for
     * mapping routes, mounting middleware, and starting the HTTP loop.
     * Construct one via Skirnir's @c skr::ApplicationBuilder and call
     * @ref Run to block until shutdown.
     */
    class WebApplication : public skr::IApplication
    {
      public:
        /**
         * @brief Construct a WebApplication rooted at the given DI container.
         *
         * @param rootServiceProvider The shared service provider that supplies
         *        the router, middleware factory list, logger and other
         *        framework services.
         */
        explicit WebApplication(
            const skr::Arc<skr::ServiceProvider>& rootServiceProvider);

        /**
         * @brief Begin registering a GET handler for @p route.
         *
         * @param route A router path template (e.g. @c "/users/:id").
         * @return A @ref RouteRegistration builder used to attach OpenAPI
         *         metadata, schemas, and the final handler via @c Handle().
         */
        RouteRegistration MapGet(const std::string& route);

        /**
         * @brief Register @p handler for GET @p route in a single call.
         *
         * @param route   Router path template.
         * @param handler Callable invoked when the route matches.
         */
        void MapGet(const std::string& route, auto&& handler)
        {
            MapGet(route).Handle(std::forward<decltype(handler)>(handler));
        }

        /**
         * @brief Begin registering a POST handler for @p route.
         *
         * @param route A router path template.
         * @return A @ref RouteRegistration builder.
         */
        RouteRegistration MapPost(const std::string& route);

        /**
         * @brief Register @p handler for POST @p route in a single call.
         */
        void MapPost(const std::string& route, auto&& handler)
        {
            MapPost(route).Handle(std::forward<decltype(handler)>(handler));
        }

        /**
         * @brief Begin registering a PUT handler for @p route.
         *
         * @param route A router path template.
         * @return A @ref RouteRegistration builder.
         */
        RouteRegistration MapPut(const std::string& route);

        /**
         * @brief Register @p handler for PUT @p route in a single call.
         */
        void MapPut(const std::string& route, auto&& handler)
        {
            MapPut(route).Handle(std::forward<decltype(handler)>(handler));
        }

        /**
         * @brief Begin registering a DELETE handler for @p route.
         *
         * @param route A router path template.
         * @return A @ref RouteRegistration builder.
         */
        RouteRegistration MapDelete(const std::string& route);

        /**
         * @brief Register @p handler for DELETE @p route in a single call.
         */
        void MapDelete(const std::string& route, auto&& handler)
        {
            MapDelete(route).Handle(std::forward<decltype(handler)>(handler));
        }

        /**
         * @brief Begin registering a PATCH handler for @p route.
         *
         * @param route A router path template.
         * @return A @ref RouteRegistration builder.
         */
        RouteRegistration MapPatch(const std::string& route);

        /**
         * @brief Register @p handler for PATCH @p route in a single call.
         */
        void MapPatch(const std::string& route, auto&& handler)
        {
            MapPatch(route).Handle(std::forward<decltype(handler)>(handler));
        }

        /**
         * @brief Group a set of routes under a common URL prefix.
         *
         * The @p setup callable is invoked with a @ref RouteBuilder that
         * prefixes every route it registers.
         *
         * @param prefix URL prefix shared by all routes in the group.
         * @param setup  Callable receiving the @ref RouteBuilder.
         */
        void MapGroup(const std::string& prefix, auto setup)
        {
            RouteBuilder builder(*mImpl->mRouter, prefix);
            setup(builder);
        }

        /**
         * @brief Serve static files from @p rootPath under @p urlPrefix.
         *
         * The framework resolves paths inside @p rootPath only, blocks
         * directory traversal (@c ..), sets sensible ETag/Last-Modified
         * metadata, and streams the body via chunked transfer encoding.
         *
         * @param urlPrefix  URL prefix under which the files are mounted
         *                   (e.g. @c "/assets").
         * @param rootPath   Absolute or working-directory-relative path on
         *                   disk where the files live.
         */
        void MapStaticFiles(const std::string& urlPrefix,
                            const std::string& rootPath);

        /**
         * @brief Append a middleware type to the request pipeline.
         *
         * The type must be registered in the DI container; an instance is
         * resolved per request via the request-scoped provider.
         *
         * @tparam TMiddleware A class deriving from @ref IMiddleware.
         * @return Reference to @c *this for fluent chaining.
         */
        template <typename TMiddleware>
        const WebApplication& Use()
        {
            mImpl->mMiddlewareProvider->AddMiddleware<TMiddleware>();
            return *this;
        }

        /**
         * @brief Start the HTTP server and block until shutdown.
         *
         * Invoked by Skirnir's host once configuration has finished.
         */
        void Run() override;

        /**
         * @brief Internal registration helper that bundles OpenAPI metadata
         *        with a route binding.
         *
         * Used by the @ref RouteRegistration builder when it finalises a
         * handler. Not part of the user-facing API.
         */
        template <typename Handler>
        void BindRoute(HttpMethod method, const std::string& route,
                       const std::string&  groupPrefix,
                       const RouteOptions& options,
                       const std::string&  requestSchemaJson,
                       const std::string& responseSchemaJson, Handler&& handler)
        {
            MapRoute(method, route, groupPrefix, options, handler);
        }

        /**
         * @brief Register @p handler for the given HTTP method and path.
         *
         * Convenience overload with no group prefix and default
         * @ref RouteOptions.
         */
        template <typename Handler>
        void MapRoute(HttpMethod method, const std::string& route,
                      Handler&& handler)
        {
            MapRoute(method, route, std::string {}, RouteOptions {},
                     std::forward<Handler>(handler));
        }

        /**
         * @brief Register @p handler with explicit OpenAPI options.
         *
         * The framework inspects the handler signature and resolves any
         * non-@ref HttpRequest / @ref HttpResponse arguments from the
         * request-scoped service provider. The handler's return value is
         * serialised to JSON (or assigned to @c body as text), or rendered
         * through @ref IResult / @ref IStreamingResult when applicable.
         */
        template <typename Handler>
        void MapRoute(HttpMethod method, const std::string& route,
                      const std::string&  groupPrefix,
                      const RouteOptions& options, Handler&& handler)
        {
            mImpl->mRouter->insert(
                method,
                route,
                options,
                groupPrefix,
                [handler = std::forward<Handler>(handler),
                 this](HttpRequest&  request,
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
                        (detail::BindOneBody<I, HandlerArgsTuple>(
                             boundBodies, request),
                         ...);
                    }(std::make_index_sequence<N> {});

                    auto args =
                        detail::BuildArgsTuple<HandlerArgsTuple>([&](auto tag) {
                            using TArg = typename decltype(tag)::type;
                            if constexpr (std::is_same_v<TArg, HttpRequest>)
                            {
                                return request;
                            }
                            else if constexpr (std::is_same_v<TArg,
                                                              HttpResponse>)
                            {
                                return response;
                            }
                            else if constexpr (isFromBody_v<TArg>)
                            {
                                constexpr std::size_t Idx =
                                    detail::IndexOfFromBody<HandlerArgsTuple,
                                                            TArg>::value;
                                return std::get<Idx>(boundBodies);
                            }
                            else
                            {
                                return *serviceProvider->GetService<TArg>();
                            }
                        });

                    using ResultType = LambdaResult<decltype(handler)>;
                    if constexpr (!std::is_same_v<ResultType, void>)
                    {
                        auto result = std::move(std::apply(handler, args));

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

      public:
        /**
         * @brief Scoped builder used inside @ref MapGroup.
         *
         * Routes registered through a @c RouteBuilder are prefixed with
         * the group's path. The prefix and the per-route template are
         * concatenated by the internal @c join helper, which handles the
         * leading/trailing slash cases (e.g. prefix @c "/api" + route
         * @c "/users/:id" -> @c "/api/users/:id").
         */
        class RouteBuilder
        {
          public:
            /**
             * @brief Construct a builder that owns @p router and prefixes
             *        routes with @p prefix.
             */
            RouteBuilder(Router& router, std::string prefix) :
                mRouter(router), mPrefix(std::move(prefix))
            {
            }

            /**
             * @brief Begin a GET registration under the group's prefix.
             *
             * @param route A path template appended to the group prefix.
             * @return A @ref RouteRegistration builder.
             */
            RouteRegistration MapGet(const std::string& route);

            /**
             * @brief Register @p handler for GET @p route under the group.
             */
            void MapGet(const std::string& route, auto&& handler)
            {
                MapGet(route).Handle(std::forward<decltype(handler)>(handler));
            }

            /**
             * @brief Begin a POST registration under the group's prefix.
             *
             * @param route A path template appended to the group prefix.
             * @return A @ref RouteRegistration builder.
             */
            RouteRegistration MapPost(const std::string& route);

            /**
             * @brief Register @p handler for POST @p route under the group.
             */
            void MapPost(const std::string& route, auto&& handler)
            {
                MapPost(route).Handle(std::forward<decltype(handler)>(handler));
            }

            /**
             * @brief Begin a PUT registration under the group's prefix.
             *
             * @param route A path template appended to the group prefix.
             * @return A @ref RouteRegistration builder.
             */
            RouteRegistration MapPut(const std::string& route);

            /**
             * @brief Register @p handler for PUT @p route under the group.
             */
            void MapPut(const std::string& route, auto&& handler)
            {
                MapPut(route).Handle(std::forward<decltype(handler)>(handler));
            }

            /**
             * @brief Begin a DELETE registration under the group's prefix.
             *
             * @param route A path template appended to the group prefix.
             * @return A @ref RouteRegistration builder.
             */
            RouteRegistration MapDelete(const std::string& route);

            /**
             * @brief Register @p handler for DELETE @p route under the group.
             */
            void MapDelete(const std::string& route, auto&& handler)
            {
                MapDelete(route).Handle(
                    std::forward<decltype(handler)>(handler));
            }

            /**
             * @brief Begin a PATCH registration under the group's prefix.
             *
             * @param route A path template appended to the group prefix.
             * @return A @ref RouteRegistration builder.
             */
            RouteRegistration MapPatch(const std::string& route);

            /**
             * @brief Register @p handler for PATCH @p route under the group.
             */
            void MapPatch(const std::string& route, auto&& handler)
            {
                MapPatch(route).Handle(
                    std::forward<decltype(handler)>(handler));
            }

          private:
            static std::string join(const std::string& a, const std::string& b);

            Router&     mRouter;
            std::string mPrefix;
        };

        /**
         * @brief Access the underlying router (useful for diagnostics or
         *        imperative route inspection from extensions such as the
         *        OpenAPI spec service).
         */
        skr::Arc<Router> GetRouter() const;

      private:
        std::unique_ptr<detail::WebApplicationImpl> mImpl;
    };

} // namespace BALDR_NAMESPACE

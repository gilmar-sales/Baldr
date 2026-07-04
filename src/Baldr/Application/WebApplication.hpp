#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Tuple.hpp>

#include "WebApplication_Impl.hpp"

namespace BALDR_NAMESPACE
{

    class WebApplication : public skr::IApplication
    {
      public:
        explicit WebApplication(
            const skr::Arc<skr::ServiceProvider>& rootServiceProvider);

        RouteRegistration MapGet(const std::string& route);

        void MapGet(const std::string& route, auto&& handler)
        {
            MapGet(route).Handle(std::forward<decltype(handler)>(handler));
        }

        RouteRegistration MapPost(const std::string& route);

        void MapPost(const std::string& route, auto&& handler)
        {
            MapPost(route).Handle(std::forward<decltype(handler)>(handler));
        }

        RouteRegistration MapPut(const std::string& route);

        void MapPut(const std::string& route, auto&& handler)
        {
            MapPut(route).Handle(std::forward<decltype(handler)>(handler));
        }

        RouteRegistration MapDelete(const std::string& route);

        void MapDelete(const std::string& route, auto&& handler)
        {
            MapDelete(route).Handle(std::forward<decltype(handler)>(handler));
        }

        RouteRegistration MapPatch(const std::string& route);

        void MapPatch(const std::string& route, auto&& handler)
        {
            MapPatch(route).Handle(std::forward<decltype(handler)>(handler));
        }

        void MapGroup(const std::string& prefix, auto setup)
        {
            RouteBuilder builder(*mImpl->mRouter, prefix);
            setup(builder);
        }

        void MapStaticFiles(const std::string& urlPrefix,
                            const std::string& rootPath);

        template <typename TMiddleware>
        const WebApplication& Use()
        {
            mImpl->mMiddlewareProvider->AddMiddleware<TMiddleware>();
            return *this;
        }

        void Run() override;

        template <typename Handler>
        void BindRoute(HttpMethod method, const std::string& route,
                       const std::string&  groupPrefix,
                       const RouteOptions& options,
                       const std::string&  requestSchemaJson,
                       const std::string& responseSchemaJson, Handler&& handler)
        {
            MapRoute(method, route, groupPrefix, options, handler);
        }

        template <typename Handler>
        void MapRoute(HttpMethod method, const std::string& route,
                      Handler&& handler)
        {
            MapRoute(method, route, std::string {}, RouteOptions {},
                     std::forward<Handler>(handler));
        }

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

                    auto refFactory = [&]<typename TArg>(TArg* x) -> TArg& {
                        if constexpr (std::is_same_v<HttpRequest, TArg>)
                        {
                            return request;
                        }

                        if constexpr (std::is_same_v<HttpResponse, TArg>)
                        {
                            return response;
                        }

                        return *serviceProvider->GetService<TArg>();
                    };

                    auto ptrFactory = [&]<typename TArg>(
                                          skr::Arc<TArg>* x) -> skr::Arc<TArg> {
                        return serviceProvider
                            ->GetService<std::remove_pointer_t<TArg>>();
                    };

                    auto args = transformTuple<HandlerArgsTuple>(
                        refFactory, ptrFactory,
                        TupleOfPtr((HandlerArgsTuple*) nullptr));

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
        class RouteBuilder
        {
          public:
            RouteBuilder(Router& router, std::string prefix) :
                mRouter(router), mPrefix(std::move(prefix))
            {
            }

            RouteRegistration MapGet(const std::string& route);

            void MapGet(const std::string& route, auto&& handler)
            {
                MapGet(route).Handle(std::forward<decltype(handler)>(handler));
            }

            RouteRegistration MapPost(const std::string& route);

            void MapPost(const std::string& route, auto&& handler)
            {
                MapPost(route).Handle(std::forward<decltype(handler)>(handler));
            }

            RouteRegistration MapPut(const std::string& route);

            void MapPut(const std::string& route, auto&& handler)
            {
                MapPut(route).Handle(std::forward<decltype(handler)>(handler));
            }

            RouteRegistration MapDelete(const std::string& route);

            void MapDelete(const std::string& route, auto&& handler)
            {
                MapDelete(route).Handle(
                    std::forward<decltype(handler)>(handler));
            }

            RouteRegistration MapPatch(const std::string& route);

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

        skr::Arc<Router> GetRouter() const;

      private:
        std::unique_ptr<detail::WebApplicationImpl> mImpl;
    };

} // namespace BALDR_NAMESPACE
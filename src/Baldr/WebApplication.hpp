#pragma once

#include <Skirnir/Skirnir.hpp>

#include "MiddlewareProvider.hpp"
#include "Result.hpp"
#include "RouteOptions.hpp"
#include "RouteRegistration.hpp"
#include "StreamingResult.hpp"
#include "Tuple.hpp"

class WebApplication : public skr::IApplication
{
  public:
    WebApplication(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider),
        mRouter(rootServiceProvider->GetService<Router>()),
        mMiddlewareProvider(
            rootServiceProvider->GetService<MiddlewareProvider>())
    {
    }

    Baldr::RouteRegistration MapGet(const std::string& route)
    {
        return Baldr::RouteRegistration(*mRouter, HttpMethod::Get, route);
    }

    void MapGet(const std::string& route, auto&& handler)
    {
        MapGet(route).Handle(std::forward<decltype(handler)>(handler));
    }

    Baldr::RouteRegistration MapPost(const std::string& route)
    {
        return Baldr::RouteRegistration(*mRouter, HttpMethod::Post, route);
    }

    void MapPost(const std::string& route, auto&& handler)
    {
        MapPost(route).Handle(std::forward<decltype(handler)>(handler));
    }

    Baldr::RouteRegistration MapPut(const std::string& route)
    {
        return Baldr::RouteRegistration(*mRouter, HttpMethod::Put, route);
    }

    void MapPut(const std::string& route, auto&& handler)
    {
        MapPut(route).Handle(std::forward<decltype(handler)>(handler));
    }

    Baldr::RouteRegistration MapDelete(const std::string& route)
    {
        return Baldr::RouteRegistration(*mRouter, HttpMethod::Delete, route);
    }

    void MapDelete(const std::string& route, auto&& handler)
    {
        MapDelete(route).Handle(std::forward<decltype(handler)>(handler));
    }

    Baldr::RouteRegistration MapPatch(const std::string& route)
    {
        return Baldr::RouteRegistration(*mRouter, HttpMethod::Patch, route);
    }

    void MapPatch(const std::string& route, auto&& handler)
    {
        MapPatch(route).Handle(std::forward<decltype(handler)>(handler));
    }

    void MapGroup(const std::string& prefix, auto setup)
    {
        RouteBuilder builder(*mRouter, prefix);
        setup(builder);
    }

    void MapStaticFiles(const std::string& urlPrefix,
                        const std::string& rootPath);

    template <typename TMiddleware>
    const WebApplication& Use()
    {
        mMiddlewareProvider->AddMiddleware<TMiddleware>();

        return *this;
    }

    void Run() override;

    // Public so RouteBuilder and RouteRegistration can call into it for
    // grouped routes and per-route fluent options. Forwards to MapRoute.
    template <typename Handler>
    void BindRoute(HttpMethod method, const std::string& route,
                   const std::string&         groupPrefix,
                   const Baldr::RouteOptions& options,
                   const std::string&         requestSchemaJson,
                   const std::string& responseSchemaJson, Handler&& handler)
    {
        MapRoute(method, route, groupPrefix, options, handler);
    }

    // Public so RouteBuilder can call into it for grouped routes.
    template <typename Handler>
    void MapRoute(HttpMethod method, const std::string& route,
                  Handler&& handler)
    {
        MapRoute(method, route, std::string {}, Baldr::RouteOptions {},
                 std::forward<Handler>(handler));
    }

    template <typename Handler>
    void MapRoute(HttpMethod method, const std::string& route,
                  const std::string&         groupPrefix,
                  const Baldr::RouteOptions& options, Handler&& handler)
    {
        mRouter->insert(
            method,
            route,
            options,
            groupPrefix,
            [handler = std::forward<Handler>(handler), this](
                HttpRequest&                          request,
                HttpResponse&                         response,
                const skr::Arc<skr::ServiceProvider>& serviceProvider) mutable {
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

                auto ptrFactory =
                    [&]<typename TArg>(skr::Arc<TArg>* x) -> skr::Arc<TArg> {
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
                    else if constexpr (std::is_base_of_v<IResult, ResultType>)
                    {
                        result.Apply(response);
                    }
                    else if constexpr (std::is_same_v<const char*,
                                                      ResultType> ||
                                       std::is_same_v<char*, ResultType>)
                    {
                        response.headers["Content-Type"] = "text/plain";
                        response.body                    = std::string(result);
                        response.statusCode              = StatusCode::OK;
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
                                "Handler returned a value that could not be "
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

        Baldr::RouteRegistration MapGet(const std::string& route)
        {
            return Baldr::RouteRegistration(
                mRouter, HttpMethod::Get, join(mPrefix, route), mPrefix);
        }

        void MapGet(const std::string& route, auto&& handler)
        {
            MapGet(route).Handle(std::forward<decltype(handler)>(handler));
        }

        Baldr::RouteRegistration MapPost(const std::string& route)
        {
            return Baldr::RouteRegistration(
                mRouter, HttpMethod::Post, join(mPrefix, route), mPrefix);
        }

        void MapPost(const std::string& route, auto&& handler)
        {
            MapPost(route).Handle(std::forward<decltype(handler)>(handler));
        }

        Baldr::RouteRegistration MapPut(const std::string& route)
        {
            return Baldr::RouteRegistration(
                mRouter, HttpMethod::Put, join(mPrefix, route), mPrefix);
        }

        void MapPut(const std::string& route, auto&& handler)
        {
            MapPut(route).Handle(std::forward<decltype(handler)>(handler));
        }

        Baldr::RouteRegistration MapDelete(const std::string& route)
        {
            return Baldr::RouteRegistration(
                mRouter, HttpMethod::Delete, join(mPrefix, route), mPrefix);
        }

        void MapDelete(const std::string& route, auto&& handler)
        {
            MapDelete(route).Handle(std::forward<decltype(handler)>(handler));
        }

        Baldr::RouteRegistration MapPatch(const std::string& route)
        {
            return Baldr::RouteRegistration(
                mRouter, HttpMethod::Patch, join(mPrefix, route), mPrefix);
        }

        void MapPatch(const std::string& route, auto&& handler)
        {
            MapPatch(route).Handle(std::forward<decltype(handler)>(handler));
        }

      private:
        static std::string join(const std::string& a, const std::string& b)
        {
            if (a.empty())
                return b;
            if (b.empty())
                return a;
            if (a.back() == '/' && b.front() == '/')
                return a + b.substr(1);
            if (a.back() == '/' || b.front() == '/')
                return a + b;
            return a + "/" + b;
        }

        Router&     mRouter;
        std::string mPrefix;
    };

  private:
    skr::Arc<Router>             mRouter;
    skr::Arc<MiddlewareProvider> mMiddlewareProvider;

  public:
    skr::Arc<Router> GetRouter() const { return mRouter; }
};

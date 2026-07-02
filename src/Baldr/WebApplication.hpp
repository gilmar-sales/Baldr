#pragma once

#include <Skirnir/Skirnir.hpp>

#include "MiddlewareProvider.hpp"
#include "Result.hpp"
#include "Router.hpp"
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

    void MapGet(const std::string& route, auto&& handler)
    {
        MapRoute(HttpMethod::Get, route, handler);
    }

    void MapPost(const std::string& route, auto&& handler)
    {
        MapRoute(HttpMethod::Post, route, handler);
    }

    void MapPut(const std::string& route, auto&& handler)
    {
        MapRoute(HttpMethod::Put, route, handler);
    }

    void MapDelete(const std::string& route, auto&& handler)
    {
        MapRoute(HttpMethod::Delete, route, handler);
    }

    void MapPatch(const std::string& route, auto&& handler)
    {
        MapRoute(HttpMethod::Patch, route, handler);
    }

    // Route groups: apply a prefix to all routes registered inside `setup`.
    // The callback receives a `RouteBuilder` that exposes the same
    // MapGet/MapPost/... methods, so call sites read naturally:
    //
    //   app.MapGroup("/api/v1", [](auto& group) {
    //       group.MapGet("/users", ...);
    //   });
    void MapGroup(const std::string& prefix, auto setup)
    {
        RouteBuilder builder(*this, prefix);
        setup(builder);
    }

    // Serve static files from `rootPath` under `urlPrefix`. Mime type is
    // detected from the file extension. Arbitrarily deep sub-paths are
    // supported (`urlPrefix/**`). Directory requests fall back to
    // `index.html` when present. Path-traversal attempts (raw and
    // percent-encoded, including backslash and NUL bytes) are rejected
    // before any filesystem call.
    void MapStaticFiles(const std::string& urlPrefix,
                        const std::string& rootPath);

    template <typename TMiddleware>
    const WebApplication& Use()
    {
        mMiddlewareProvider->AddMiddleware<TMiddleware>();

        return *this;
    }

    void Run() override;

    // Public so RouteBuilder can call into it for grouped routes.
    template <typename Handler>
    void MapRoute(HttpMethod method, const std::string& route,
                  Handler&&   handler)
    {
        mRouter->insert(
            method,
            route,
            [handler = std::forward<Handler>(handler), this](
                HttpRequest&                       request,
                HttpResponse&                      response,
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

                    if constexpr (std::is_base_of_v<IResult, ResultType>)
                    {
                        result.Apply(response);
                    }
                    else if constexpr (std::is_same_v<const char*, ResultType> ||
                                        std::is_same_v<char*, ResultType>)
                    {
                        response.headers["Content-Type"] = "text/plain";
                        response.body = std::string(result);
                        response.statusCode = StatusCode::OK;
                    }
                    else if constexpr (std::is_assignable_v<std::string,
                                                             ResultType>)
                    {
                        response.headers["Content-Type"] = "text/plain";
                        response.body                    = result;
                        response.statusCode = StatusCode::OK;
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
        RouteBuilder(WebApplication& app, std::string prefix) :
            mApp(app), mPrefix(std::move(prefix))
        {
        }

        void MapGet(const std::string& route, auto&& handler)
        {
            mApp.MapRoute(HttpMethod::Get, join(mPrefix, route), handler);
        }

        void MapPost(const std::string& route, auto&& handler)
        {
            mApp.MapRoute(HttpMethod::Post, join(mPrefix, route), handler);
        }

        void MapPut(const std::string& route, auto&& handler)
        {
            mApp.MapRoute(HttpMethod::Put, join(mPrefix, route), handler);
        }

        void MapDelete(const std::string& route, auto&& handler)
        {
            mApp.MapRoute(HttpMethod::Delete, join(mPrefix, route), handler);
        }

        void MapPatch(const std::string& route, auto&& handler)
        {
            mApp.MapRoute(HttpMethod::Patch, join(mPrefix, route), handler);
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

        WebApplication& mApp;
        std::string     mPrefix;
    };

  private:
    skr::Arc<Router>             mRouter;
    skr::Arc<MiddlewareProvider> mMiddlewareProvider;
};

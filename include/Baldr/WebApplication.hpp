#pragma once

#include <Skirnir/ServiceCollection.hpp>
#include <Skirnir/ServiceProvider.hpp>

#include <rfl.hpp>
#include <rfl/json.hpp>

#include "MiddlewareProvider.hpp"
#include "PathMatcher.hpp"
#include "Tuple.hpp"

class WebApplicationBuilder;

class WebApplication
{
  public:
    explicit WebApplication(
        const Ref<skr::ServiceCollection>& serviceCollection) :
        mServiceCollection(serviceCollection),
        mPathMatcher(skr::MakeRef<PathMatcher>()),
        mMiddlewareProvider(skr::MakeRef<MiddlewareProvider>())
    {
        mServiceCollection->AddTransient<skr::Logger<WebApplication>>();
        mServiceCollection->AddSingleton(mMiddlewareProvider);
        mServiceCollection->AddSingleton(mPathMatcher);
    }

    void MapGet(const std::string& route, auto&& handler)
    {
        MapRoute("GET", route, handler);
    }

    void MapPost(const std::string& route, auto&& handler)
    {
        MapRoute("POST", route, handler);
    }

    template <typename TMiddleware>
    const WebApplication& Use()
    {
        mServiceCollection->AddScoped<TMiddleware>();

        mMiddlewareProvider->AddMiddleware<TMiddleware>();

        return *this;
    }

    void Run() const;

    static WebApplicationBuilder CreateBuilder();

  private:
    void MapRoute(const std::string& method, const std::string& route,
                  auto&& handler)
    {
        mPathMatcher->insert(
            method,
            route,
            [&](HttpRequest& request, HttpResponse& response,
                Ref<skr::ServiceProvider> serviceProvider) {
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
                    [&]<typename TArg>(Ref<TArg>* x) -> Ref<TArg> {
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

                    if constexpr (std::is_assignable_v<std::string, ResultType>)
                    {
                        response.headers["Content-Type"] = "plain/text";
                        response.body                    = result;
                    }
                    else
                    {
                        response.body = rfl::json::write(result);
                        response.headers["Content-Type"] = "application/json";
                    }

                    response.statusCode = StatusCode::OK;
                }
                else
                {
                    std::apply(handler, args);
                }
            });
    }

    Ref<skr::ServiceCollection> mServiceCollection;
    Ref<PathMatcher>            mPathMatcher;
    Ref<MiddlewareProvider>     mMiddlewareProvider;
};

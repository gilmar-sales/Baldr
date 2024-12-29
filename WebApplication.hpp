#pragma once

#include <ServiceCollection.hpp>
#include <ServiceProvider.hpp>

#include <rfl/json.hpp>
#include <rfl.hpp>

#include "IMiddleware.hpp"
#include "PathMatcher.hpp"
#include "Tuple.hpp"

class WebApplicationBuilder;

class WebApplication
{
  public:
    explicit WebApplication(
        const std::shared_ptr<ServiceCollection>& serviceCollection) :
        mServiceCollection(serviceCollection),
        mMiddlewareFactories(
            std::make_shared<std::vector<MiddlewareFactory>>()),
        mPathMatcher(std::make_shared<PathMatcher>())
    {
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
    void Use()
    {
        mServiceCollection->AddScoped<TMiddleware>();

        mMiddlewareFactories->push_back(
            [](const std::shared_ptr<ServiceProvider>& serviceProvider) {
                return serviceProvider->GetService<TMiddleware>();
            });
    }

    void Run() const;

    static WebApplicationBuilder CreateBuilder();

private:

    void MapRoute(const std::string& method,const std::string& route, auto&& handler)
    {
        mPathMatcher->insert(method,
            route, [&](HttpRequest& request, HttpResponse& response,
                       std::shared_ptr<ServiceProvider> serviceProvider) {
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
                    [&]<typename TArg>(std::shared_ptr<TArg>* x) -> std::shared_ptr<TArg> {
                    return serviceProvider->GetService<std::remove_pointer_t<TArg>>();
                };

                auto args =
                    transformTuple<HandlerArgsTuple>(refFactory, ptrFactory, TupleOfPtr((HandlerArgsTuple*) nullptr));

                if constexpr (!std::is_same_v<LambdaResult<decltype(handler)>, void>)
                {
                    auto result = std::apply(handler, args);

                    response.body = rfl::json::write(result);
                    response.headers["Content-Type"] = "application/json";
                    response.statusCode = StatusCode::OK;
                }
                else
                {
                    std::apply(handler, args);
                }
            });
    }

    std::shared_ptr<ServiceCollection>     mServiceCollection;
    std::shared_ptr<MiddlewareFactoryList> mMiddlewareFactories;
    std::shared_ptr<PathMatcher>           mPathMatcher;
};

#pragma once

#include <ServiceCollection.hpp>
#include <ServiceProvider.hpp>
#include <iostream>

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
        mPathMatcher->insert(
            route, [&](HttpRequest& request, HttpResponse& response,
                       std::shared_ptr<ServiceProvider> serviceProvider) {
                using HandlerArgsTuple = typename LambdaTraits<
                    std::remove_reference_t<decltype(handler)>>::ArgsTuple;

                // TODO: support std::shared_ptr for services
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

                std::apply(handler, args);
            });
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

    std::shared_ptr<ServiceCollection>     mServiceCollection;
    std::shared_ptr<MiddlewareFactoryList> mMiddlewareFactories;
    std::shared_ptr<PathMatcher>           mPathMatcher;
};

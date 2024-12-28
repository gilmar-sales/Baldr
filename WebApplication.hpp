#pragma once

#include <iostream>
#include <ServiceCollection.hpp>
#include <ServiceProvider.hpp>

#include "IMiddleware.hpp"
#include "PathMatcher.hpp"
#include "Tuple.hpp"

class WebApplicationBuilder;

class WebApplication {
public:
    explicit WebApplication(const std::shared_ptr<ServiceCollection> &serviceCollection)
        : mServiceCollection(serviceCollection),
          mMiddlewareFactories(std::make_shared<std::vector<MiddlewareFactory> >()),
          mPathMatcher(std::make_shared<PathMatcher>()) {
    }

    void MapGet(const std::string &route, auto &&handler) {
        mPathMatcher->insert(route, [&](HttpRequest &request, HttpResponse &response,
                                        std::shared_ptr<ServiceProvider> serviceProvider) {
            handler(request, response);
            using HandlerArgsTuple = typename LambdaTraits<std::remove_reference_t<decltype(handler)>>::ArgsTuple;
        });
    }

    template<typename... Args>
    void CallHandler(std::tuple<Args...> args) {

    }


    template<typename TMiddleware>
    void Use() {
        mServiceCollection->AddScoped<TMiddleware>();

        mMiddlewareFactories->push_back([](const std::shared_ptr<ServiceProvider> &serviceProvider) {
            return serviceProvider->GetService<TMiddleware>();
        });
    }

    void Run() const;

    static WebApplicationBuilder CreateBuilder();

private:
    std::shared_ptr<ServiceCollection> mServiceCollection;
    std::shared_ptr<MiddlewareFactoryList> mMiddlewareFactories;
    std::shared_ptr<PathMatcher> mPathMatcher;
};

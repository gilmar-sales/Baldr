#pragma once

#include <ServiceCollection.hpp>
#include <ServiceProvider.hpp>

#include "IMiddleware.hpp"

class WebApplicationBuilder;

class WebApplication {
public:
    explicit WebApplication(const std::shared_ptr<ServiceCollection> &serviceCollection)
        : mServiceCollection(serviceCollection),
          mMiddlewareFactories(std::make_shared<std::vector<MiddlewareFactory> >()) {
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
};

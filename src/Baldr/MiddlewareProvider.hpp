#pragma once

#include "IMiddleware.hpp"

using MiddlewareFactory =
    std::function<skr::Arc<IMiddleware>(const skr::Arc<skr::ServiceProvider>&)>;

using MiddlewareFactoryList = std::vector<MiddlewareFactory>;

class MiddlewareProvider
{
  public:
    template <typename TMiddleware>
    void AddMiddleware()
    {
        mMiddlewareFactories.push_back(
            [](const skr::Arc<skr::ServiceProvider>& serviceProvider) {
                return serviceProvider->GetService<TMiddleware>();
            });
    }

    const auto begin() { return mMiddlewareFactories.begin(); }

    const auto end() { return mMiddlewareFactories.end(); }

    size_t Size() { return mMiddlewareFactories.size(); }

  private:
    MiddlewareFactoryList mMiddlewareFactories;
};
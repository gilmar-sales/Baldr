#pragma once

#include <stdexcept>
#include <vector>

#include <Baldr/Middleware/IMiddleware.hpp>

using MiddlewareFactory =
    std::function<skr::Arc<IMiddleware>(const skr::Arc<skr::ServiceProvider>&)>;

using MiddlewareFactoryList = std::vector<MiddlewareFactory>;

class MiddlewareProvider
{
  public:
    template <typename TMiddleware>
    void AddMiddleware()
    {
        if (IsSealed())
            throw std::logic_error(
                "MiddlewareProvider is sealed; cannot add more middleware.");

        mMiddlewareFactories.push_back(
            [](const skr::Arc<skr::ServiceProvider>& serviceProvider) {
                return serviceProvider->GetService<TMiddleware>();
            });
    }

    // Snapshot the factory list and forbid further AddMiddleware calls.
    // Intended to be called once during application startup, before the
    // HTTP server starts serving requests. After Seal(), Factories()
    // returns the immutable snapshot, removing per-request contention on
    // the underlying container.
    void Seal() { mSealedFactories = mMiddlewareFactories; }

    bool IsSealed() const { return !mSealedFactories.empty(); }

    const auto begin() { return Factories().begin(); }

    const auto end() { return Factories().end(); }

    size_t Size() { return Factories().size(); }

    MiddlewareFactoryList& Factories() { return mSealedFactories; }

  private:
    MiddlewareFactoryList mMiddlewareFactories;
    MiddlewareFactoryList mSealedFactories;
};
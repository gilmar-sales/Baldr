/**
 * @file Middleware/MiddlewareProvider.hpp
 * @brief Ordered list of middleware types with a startup-time seal that
 *        takes a thread-safe snapshot for the hot path.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <stdexcept>
#include <vector>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Factory that builds a middleware instance from a request-scoped
     *        service provider.
     *
     * Resolved once per request, so middleware may safely hold per-request
     * state without locking.
     */
    using MiddlewareFactory = std::function<skr::Arc<IMiddleware>(
        const skr::Arc<skr::ServiceProvider>&)>;

    /// Ordered list of middleware factories, iterated per request.
    using MiddlewareFactoryList = std::vector<MiddlewareFactory>;

    /**
     * @brief Holds the application's middleware pipeline.
     *
     * Types are added via @ref AddMiddleware during startup. @ref Seal
     * takes a snapshot of the factory list and forbids further additions so
     * the hot path can iterate an immutable container without locking.
     */
    class MiddlewareProvider
    {
      public:
        /**
         * @brief Register @c TMiddleware as the next link in the chain.
         *
         * The type must already be registered with the DI container.
         *
         * @tparam TMiddleware A class deriving from @ref IMiddleware.
         * @throws std::logic_error If @ref Seal has already been called.
         */
        template <typename TMiddleware>
        void AddMiddleware()
        {
            if (IsSealed())
                throw std::logic_error("MiddlewareProvider is sealed; cannot "
                                       "add more middleware.");

            mMiddlewareFactories.push_back(
                [](const skr::Arc<skr::ServiceProvider>& serviceProvider) {
                    return serviceProvider->GetService<TMiddleware>();
                });
        }

        /**
         * @brief Snapshot the factory list and forbid further
         *        @ref AddMiddleware calls.
         *
         * Intended to be called once during application startup, before the
         * HTTP server starts serving requests. After @ref Seal,
         * @ref Factories returns the immutable snapshot, removing
         * per-request contention on the underlying container.
         */
        void Seal() { mSealedFactories = mMiddlewareFactories; }

        /// @return @c true once @ref Seal has been called.
        bool IsSealed() const { return !mSealedFactories.empty(); }

        /// @brief Begin iterator over the sealed factory list.
        const auto begin() { return Factories().begin(); }
        /// @brief End iterator over the sealed factory list.
        const auto end() { return Factories().end(); }
        /// @return Number of registered middleware.
        size_t Size() { return Factories().size(); }

        /// @return The sealed factory list (mutable reference for the
        ///         connection layer to iterate).
        MiddlewareFactoryList& Factories() { return mSealedFactories; }

      private:
        MiddlewareFactoryList mMiddlewareFactories;
        MiddlewareFactoryList mSealedFactories;
    };

} // namespace BALDR_NAMESPACE
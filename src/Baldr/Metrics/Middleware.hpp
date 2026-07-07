/**
 * @file Metrics/Middleware.hpp
 * @brief Middleware that records per-request metrics, plus a convenience
 *        helper that mounts a @c /metrics endpoint.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Configuration for @ref MetricsMiddleware and @ref MapMetrics.
     */
    struct MetricsOptions
    {
        /**
         * @brief Path where the @c /metrics endpoint will be registered.
         *        Set to empty to disable mounting (metrics are still
         *        collected internally).
         */
        std::string endpointPath = "/metrics";

        /**
         * @brief When @c true, use the request path as the latency
         *        histogram label. Set to @c false and provide a custom
         *        label upstream if your URL space is too high-cardinality.
         */
        bool usePathLabel = true;
    };

    /**
     * @brief Middleware that records per-request metrics in the process-wide
     * @ref MetricsRegistry.
     *
     * Designed to be registered as a service through DI; the
     * @c BaldrExtension opts in via @c app.Use<MetricsMiddleware>() and
     * @ref MapMetrics.
     */
    class MetricsMiddleware final : public IMiddleware
    {
      public:
        /**
         * @brief Construct the middleware with the given options.
         */
        explicit MetricsMiddleware(MetricsOptions options = {}) :
            mOptions(std::move(options))
        {
        }

        ~MetricsMiddleware() override = default;

        void Handle(HttpRequest&          request,
                    HttpResponse&         response,
                    const NextMiddleware& next) override;

      private:
        MetricsOptions mOptions;
    };

    /**
     * @brief Convenience helper that registers a @c /metrics endpoint on
     * @p app serving the current snapshot from the @ref MetricsRegistry.
     *
     * The handler runs after the chain completes, so counters reflect the
     * final status code.
     */
    void MapMetrics(class WebApplication& app, MetricsOptions options = {});

} // namespace BALDR_NAMESPACE

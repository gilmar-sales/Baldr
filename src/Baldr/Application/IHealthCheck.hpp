/**
 * @file Application/IHealthCheck.hpp
 * @brief Pluggable health probe resolved from the DI container.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string_view>

#include <Baldr/Application/HealthCheckResult.hpp>
#include <Baldr/Http/Request.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Pluggable health predicate registered with the DI container.
     *
     * Implementations are resolved via
     * @c skr::ServiceProvider::GetServices<IHealthCheck>() when a
     * health endpoint is mapped, so a single check can be registered
     * once and reused across readiness, liveness and any custom probes.
     *
     * @c CheckName is surfaced verbatim in the JSON response body and
     * must therefore be stable across requests (Kubernetes probes key
     * off it). @c Check is invoked synchronously on the request thread
     * for every probe request, so it must be cheap and non-blocking.
     * Slow probes (database pings, remote calls) should cache their
     * last known result and use a short timeout.
     *
     * The return value is a @ref HealthCheckResult. Aggregate severity
     * determines the HTTP code (Healthy and Degraded -> 200, Unhealthy
     * -> 503) and per-check detail is rendered in the JSON body.
     * Throwing from @c Check is treated as an Unhealthy result with
     * the exception @c what() (or a generic message) as the error.
     *
     * @code
     * class DatabaseHealthCheck : public baldr::IHealthCheck
     * {
     *   public:
     *     std::string_view CheckName() const noexcept override
     *     {
     *         return "database";
     *     }
     *
     *     HealthCheckResult Check(const baldr::HttpRequest&) override
     *     {
     *         return mLastPingOk.load()
     *                    ? HealthCheckResult::Healthy("primary db")
     *                    : HealthCheckResult::Unhealthy(
     *                          "primary db", "ping failed");
     *     }
     *
     *   private:
     *     std::atomic<bool> mLastPingOk { true };
     * };
     *
     * builder.GetServiceCollection()->AddSingleton<IHealthCheck,
     *     DatabaseHealthCheck>();
     * @endcode
     */
    class IHealthCheck
    {
      public:
        /**
         * @brief Virtual destructor so derived classes are destroyed
         *        through an @c IHealthCheck pointer.
         */
        virtual ~IHealthCheck() = default;

        /**
         * @brief Stable identifier surfaced in the JSON body.
         *
         * The name appears as a key under @c "checks" in the probe
         * response and is part of the public contract of the probe.
         *
         * @return A view whose lifetime extends at least until the
         *         next call into the implementation; the framework
         *         copies it eagerly when resolving the check.
         */
        virtual std::string_view CheckName() const noexcept = 0;

        /**
         * @brief Run the probe for the incoming request.
         *
         * Implementations must not block for long; cache expensive
         * upstream calls and return the most recent cached value.
         *
         * @param request The inbound probe request, useful for
         *                inspecting headers (e.g. forced failures in
         *                tests) or query parameters.
         * @return A @ref HealthCheckResult describing the outcome.
         * @throws Anything — the framework catches and treats it as
         *         an @c Unhealthy result with the exception @c what()
         *         as the @c error field.
         */
        virtual HealthCheckResult Check(const HttpRequest& request) = 0;
    };

} // namespace BALDR_NAMESPACE
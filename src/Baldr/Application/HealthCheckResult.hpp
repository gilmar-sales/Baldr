/**
 * @file Application/HealthCheckResult.hpp
 * @brief Result type returned by @ref IHealthCheck::Check.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Outcome of a single @ref IHealthCheck probe.
     *
     * Ordered from least to most severe so that @c std::max-style
     * aggregation can collapse a set of results with
     * @code
     * acc = std::max(acc, r.status);
     * @endcode
     */
    enum class HealthStatus : std::uint8_t
    {
        Healthy   = 0,
        Degraded  = 1,
        Unhealthy = 2,
    };

    /**
     * @brief Rich outcome of a single @ref IHealthCheck probe.
     *
     * @c status drives HTTP-level aggregation (Healthy and Degraded both
     * yield @c 200, only @c Unhealthy flips the endpoint to @c 503). The
     * remaining fields are surfaced verbatim in the JSON body under the
     * check's name:
     *
     * @code{.json}
     * "db": {
     *   "status":      "healthy",
     *   "description": "primary db",
     *   "error":       null,
     *   "data":        { "latencyMs": 42 }
     * }
     * @endcode
     *
     * @c description is optional free text (omit when empty).
     * @c error is typically populated only for @c Unhealthy results.
     * @c data is a pre-serialized JSON fragment inlined verbatim into the
     * response — it lets callers surface structured detail (latency,
     * queue depth, replica set, ...) without forcing the framework to
     * understand the data shape.
     */
    struct HealthCheckResult
    {
        HealthStatus               status;      ///< Severity of the outcome.
        std::string                description; ///< Optional human label.
        std::optional<std::string> error;       ///< Optional failure detail.
        std::optional<std::string> data; ///< Optional pre-serialized JSON.

        /**
         * @brief Construct a @c Healthy result.
         *
         * @param description Optional human label.
         */
        static HealthCheckResult Healthy(std::string description = {})
        {
            return { HealthStatus::Healthy, std::move(description),
                     std::nullopt, std::nullopt };
        }

        /**
         * @brief Construct a @c Degraded result.
         *
         * The endpoint still returns @c 200; @c Degraded is surfaced
         * per-check. Useful for "serving but slow" conditions.
         *
         * @param description Optional human label.
         * @param error       Optional reason for the degradation.
         * @param data        Optional pre-serialized JSON detail.
         */
        static HealthCheckResult Degraded(
            std::string                description = {},
            std::optional<std::string> error       = std::nullopt,
            std::optional<std::string> data        = std::nullopt)
        {
            return { HealthStatus::Degraded, std::move(description),
                     std::move(error), std::move(data) };
        }

        /**
         * @brief Construct an @c Unhealthy result.
         *
         * Contributes to a top-level @c "unhealthy" status and a
         * @c 503 HTTP response.
         *
         * @param description Optional human label.
         * @param error       Optional reason for the failure.
         * @param data        Optional pre-serialized JSON detail.
         */
        static HealthCheckResult Unhealthy(
            std::string                description = {},
            std::optional<std::string> error       = std::nullopt,
            std::optional<std::string> data        = std::nullopt)
        {
            return { HealthStatus::Unhealthy, std::move(description),
                     std::move(error), std::move(data) };
        }
    };

} // namespace BALDR_NAMESPACE
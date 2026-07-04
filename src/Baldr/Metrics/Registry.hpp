/**
 * @file Metrics/Registry.hpp
 * @brief Process-wide Prometheus-flavoured metrics registry.
 *
 * Counters (request count, in-flight requests) and per-route latency
 * histograms are aggregated here and rendered to the Prometheus text
 * format by @ref renderPrometheus.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace BALDR_NAMESPACE
{

    class MetricsRegistry;

    namespace detail
    {
        struct MetricsRegistryImpl;
    } // namespace detail

    /**
     * @brief Singleton holding counter and histogram metrics.
     *
     * The default instance is obtained via @ref instance. Tests construct
     * private instances through the @c TestOnlyTag overload.
     */
    class MetricsRegistry
    {
      public:
        /**
         * @brief Snapshot of a single histogram series at a point in time.
         */
        struct HistogramSnapshot
        {
            std::vector<double>
                upperBounds; ///< Inclusive upper bounds for each bucket.
            std::vector<std::uint64_t>
                bucketCounts; ///< Cumulative counts per bucket (Prometheus
                              ///< style).
            std::uint64_t count = 0;   ///< Total observation count.
            double        sum   = 0.0; ///< Sum of observed values.
        };

        /// @return The process-wide singleton.
        static MetricsRegistry& instance();

        /**
         * @brief Increment the request counter for @p method/@p status.
         */
        void incRequest(std::string_view method, int status);

        /**
         * @brief Observe a request latency in seconds for the given
         *        @p method/@p path pair.
         */
        void observeLatencySeconds(std::string_view method,
                                   std::string_view path,
                                   double           seconds);

        /**
         * @brief Adjust the in-flight gauge by @p delta.
         */
        void incInFlight(int delta);

        /**
         * @brief Render all metrics in Prometheus text exposition format.
         */
        std::string renderPrometheus() const;

        /// @return Total number of requests recorded so far.
        std::uint64_t requestCount() const;
        /// @return Current value of the in-flight gauge.
        std::int64_t inFlight() const;

        /// @return Default histogram bucket upper bounds (seconds).
        static const std::vector<double>& defaultBuckets();

        /// @brief Tag type selecting the test-only constructor.
        struct TestOnlyTag
        {
        };

        /**
         * @brief Test-only constructor that bypasses the singleton.
         */
        explicit MetricsRegistry(TestOnlyTag);

        /// @brief Composite key identifying a per-route histogram series.
        struct HistogramKey
        {
            std::string method; ///< HTTP method label.
            std::string path;   ///< Path label (template or normalised path).
            bool        operator==(const HistogramKey& o) const
            {
                return method == o.method && path == o.path;
            }
        };

        /// @brief Hash specialisation used by the internal @c unordered_map.
        struct HistogramKeyHash
        {
            std::size_t operator()(const HistogramKey& k) const;
        };

      private:
        MetricsRegistry();
        detail::MetricsRegistryImpl* mImpl;
    };

} // namespace BALDR_NAMESPACE
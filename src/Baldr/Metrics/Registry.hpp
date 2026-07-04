#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Baldr
{
    // Tiny process-wide metrics store. Counters are monotonic; the
    // histogram is a fixed-bucket latency histogram in seconds. Values
    // are emitted in Prometheus text format.
    class MetricsRegistry
    {
      public:
        struct HistogramSnapshot
        {
            std::vector<double>              upperBounds;
            std::vector<std::uint64_t>       bucketCounts;
            std::uint64_t                    count = 0;
            double                           sum   = 0.0;
        };

        static MetricsRegistry& instance();

        void incRequest(std::string_view method, int status);

        void observeLatencySeconds(std::string_view method,
                                   std::string_view path,
                                   double           seconds);

        void incInFlight(int delta);

        std::string renderPrometheus() const;

        std::uint64_t requestCount() const { return mRequestCount.load(); }

        std::int64_t inFlight() const { return mInFlight.load(); }

        // Default buckets in seconds (Prometheus-style latency buckets).
        static const std::vector<double>& defaultBuckets()
        {
            static const std::vector<double> buckets = {
                0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0,
                2.5,   5.0,  10.0
            };
            return buckets;
        }

      private:
        MetricsRegistry();

      public:
        // Public for tests; production code should use the singleton.
        struct TestOnlyTag {};
        explicit MetricsRegistry(TestOnlyTag) {}

        struct HistogramKey
        {
            std::string method;
            std::string path;
            bool        operator==(const HistogramKey& o) const
            {
                return method == o.method && path == o.path;
            }
        };
        struct HistogramKeyHash
        {
            std::size_t operator()(const HistogramKey& k) const
            {
                return std::hash<std::string> {}(k.method) ^
                       (std::hash<std::string> {}(k.path) << 1);
            }
        };

        mutable std::mutex mMutex;
        std::unordered_map<std::string, std::uint64_t> mStatusCounts;
        std::unordered_map<std::string, std::uint64_t> mMethodCounts;
        std::unordered_map<HistogramKey, HistogramSnapshot,
                           HistogramKeyHash>
                                                          mLatency;
        std::atomic<std::uint64_t> mRequestCount { 0 };
        std::atomic<std::int64_t>  mInFlight { 0 };
    };
} // namespace Baldr

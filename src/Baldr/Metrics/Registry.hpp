#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Baldr
{
    class MetricsRegistry;

    namespace detail
    {
        struct MetricsRegistryImpl;
    } // namespace detail

    class MetricsRegistry
    {
      public:
        struct HistogramSnapshot
        {
            std::vector<double>        upperBounds;
            std::vector<std::uint64_t> bucketCounts;
            std::uint64_t              count = 0;
            double                     sum   = 0.0;
        };

        static MetricsRegistry& instance();

        void incRequest(std::string_view method, int status);

        void observeLatencySeconds(std::string_view method,
                                   std::string_view path,
                                   double           seconds);

        void incInFlight(int delta);

        std::string renderPrometheus() const;

        std::uint64_t requestCount() const;
        std::int64_t  inFlight() const;

        static const std::vector<double>& defaultBuckets();

        struct TestOnlyTag
        {
        };

        explicit MetricsRegistry(TestOnlyTag);

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
            std::size_t operator()(const HistogramKey& k) const;
        };

      private:
        MetricsRegistry();
        detail::MetricsRegistryImpl* mImpl;
    };
} // namespace Baldr
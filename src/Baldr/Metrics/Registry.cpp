#include <Baldr/Metrics/Registry.hpp>

#include <atomic>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace Baldr::detail
{
    struct MetricsRegistryImpl
    {
        mutable std::mutex mMutex;
        std::unordered_map<std::string, std::uint64_t> mStatusCounts;
        std::unordered_map<std::string, std::uint64_t> mMethodCounts;
        std::unordered_map<MetricsRegistry::HistogramKey,
                           MetricsRegistry::HistogramSnapshot,
                           MetricsRegistry::HistogramKeyHash>
            mLatency;
        std::atomic<std::uint64_t> mRequestCount { 0 };
        std::atomic<std::int64_t>  mInFlight { 0 };
    };
} // namespace Baldr::detail

namespace Baldr
{
    std::size_t MetricsRegistry::HistogramKeyHash::operator()(
        const HistogramKey& k) const
    {
        return std::hash<std::string> {}(k.method) ^
               (std::hash<std::string> {}(k.path) << 1);
    }

    MetricsRegistry::MetricsRegistry() : mImpl(new detail::MetricsRegistryImpl) {}

    MetricsRegistry::MetricsRegistry(TestOnlyTag) :
        mImpl(new detail::MetricsRegistryImpl) {}

    MetricsRegistry& MetricsRegistry::instance()
    {
        static MetricsRegistry inst;
        return inst;
    }

    void MetricsRegistry::incRequest(std::string_view method, int status)
    {
        mImpl->mRequestCount.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(mImpl->mMutex);
        std::string m(method);
        std::string s = std::to_string(status);
        ++mImpl->mMethodCounts[m];
        ++mImpl->mStatusCounts[s];
    }

    void MetricsRegistry::observeLatencySeconds(std::string_view method,
                                                std::string_view path,
                                                double           seconds)
    {
        std::lock_guard<std::mutex> lock(mImpl->mMutex);
        HistogramKey key { std::string(method), std::string(path) };
        auto&        h = mImpl->mLatency[key];

        if (h.upperBounds.empty())
        {
            h.upperBounds  = defaultBuckets();
            h.bucketCounts.assign(h.upperBounds.size(), 0);
        }

        for (std::size_t i = 0; i < h.upperBounds.size(); ++i)
        {
            if (seconds <= h.upperBounds[i])
            {
                ++h.bucketCounts[i];
            }
        }
        ++h.count;
        h.sum += seconds;
    }

    void MetricsRegistry::incInFlight(int delta)
    {
        if (delta >= 0)
            mImpl->mInFlight.fetch_add(static_cast<std::uint64_t>(delta),
                                       std::memory_order_relaxed);
        else
            mImpl->mInFlight.fetch_sub(static_cast<std::uint64_t>(-delta),
                                       std::memory_order_relaxed);
    }

    std::string MetricsRegistry::renderPrometheus() const
    {
        std::ostringstream oss;

        oss << "# HELP baldr_http_requests_total Total HTTP requests handled\n";
        oss << "# TYPE baldr_http_requests_total counter\n";

        std::lock_guard<std::mutex> lock(mImpl->mMutex);
        std::uint64_t                totalCount = mImpl->mRequestCount.load();
        (void)totalCount;
        for (const auto& [status, count] : mImpl->mStatusCounts)
        {
            oss << "baldr_http_requests_total{status=\"" << status << "\"} "
                << count << '\n';
        }

        oss << "# HELP baldr_http_requests_by_method_total "
               "Total HTTP requests by method\n";
        oss << "# TYPE baldr_http_requests_by_method_total counter\n";
        for (const auto& [method, count] : mImpl->mMethodCounts)
        {
            oss << "baldr_http_requests_by_method_total{method=\"" << method
                << "\"} " << count << '\n';
        }

        oss << "# HELP baldr_http_request_duration_seconds "
               "Request latency histogram\n";
        oss << "# TYPE baldr_http_request_duration_seconds histogram\n";
        for (const auto& [key, h] : mImpl->mLatency)
        {
            std::uint64_t cumulative = 0;
            for (std::size_t i = 0; i < h.upperBounds.size(); ++i)
            {
                cumulative += h.bucketCounts[i];
                oss << "baldr_http_request_duration_seconds_bucket{method=\""
                    << key.method << "\",path=\"" << key.path << "\",le=\""
                    << std::fixed << std::setprecision(6) << h.upperBounds[i]
                    << "\"} " << cumulative << '\n';
            }
            cumulative += 0;
            oss << "baldr_http_request_duration_seconds_bucket{method=\""
                << key.method << "\",path=\"" << key.path
                << "\",le=\"+Inf\"} " << h.count << '\n';
            oss << "baldr_http_request_duration_seconds_count{method=\""
                << key.method << "\",path=\"" << key.path << "\"} " << h.count
                << '\n';
            oss << "baldr_http_request_duration_seconds_sum{method=\""
                << key.method << "\",path=\"" << key.path << "\"} "
                << std::fixed << std::setprecision(6) << h.sum << '\n';
        }

        oss << "# HELP baldr_http_in_flight_requests In-flight HTTP requests\n";
        oss << "# TYPE baldr_http_in_flight_requests gauge\n";
        oss << "baldr_http_in_flight_requests " << mImpl->mInFlight.load() << '\n';

        return oss.str();
    }

    std::uint64_t MetricsRegistry::requestCount() const
    {
        return mImpl->mRequestCount.load();
    }

    std::int64_t MetricsRegistry::inFlight() const
    {
        return mImpl->mInFlight.load();
    }

    const std::vector<double>& MetricsRegistry::defaultBuckets()
    {
        static const std::vector<double> buckets = {
            0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0,
            2.5,   5.0,  10.0
        };
        return buckets;
    }
} // namespace Baldr
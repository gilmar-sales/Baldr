#include <Baldr/Metrics/Registry.hpp>

#include <atomic>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Baldr
{
    MetricsRegistry::MetricsRegistry() = default;

    MetricsRegistry& MetricsRegistry::instance()
    {
        static MetricsRegistry inst;
        return inst;
    }

    void MetricsRegistry::incRequest(std::string_view method, int status)
    {
        mRequestCount.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(mMutex);
        std::string                 m(method);
        std::string                 s = std::to_string(status);
        ++mMethodCounts[m];
        ++mStatusCounts[s];
    }

    void MetricsRegistry::observeLatencySeconds(std::string_view method,
                                                std::string_view path,
                                                double           seconds)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        HistogramKey key { std::string(method), std::string(path) };
        auto&        h = mLatency[key];

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
            mInFlight.fetch_add(static_cast<std::uint64_t>(delta),
                                std::memory_order_relaxed);
        else
            mInFlight.fetch_sub(static_cast<std::uint64_t>(-delta),
                                std::memory_order_relaxed);
    }

    std::string MetricsRegistry::renderPrometheus() const
    {
        std::ostringstream oss;

        oss << "# HELP baldr_http_requests_total Total HTTP requests handled\n";
        oss << "# TYPE baldr_http_requests_total counter\n";

        std::lock_guard<std::mutex> lock(mMutex);
        std::uint64_t                totalCount = mRequestCount.load();
        (void)totalCount;
        for (const auto& [status, count] : mStatusCounts)
        {
            oss << "baldr_http_requests_total{status=\"" << status << "\"} "
                << count << '\n';
        }

        oss << "# HELP baldr_http_requests_by_method_total "
               "Total HTTP requests by method\n";
        oss << "# TYPE baldr_http_requests_by_method_total counter\n";
        for (const auto& [method, count] : mMethodCounts)
        {
            oss << "baldr_http_requests_by_method_total{method=\"" << method
                << "\"} " << count << '\n';
        }

        oss << "# HELP baldr_http_request_duration_seconds "
               "Request latency histogram\n";
        oss << "# TYPE baldr_http_request_duration_seconds histogram\n";
        for (const auto& [key, h] : mLatency)
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
            cumulative += 0; // +Inf bucket equals total count.
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
        oss << "baldr_http_in_flight_requests " << mInFlight.load() << '\n';

        return oss.str();
    }
} // namespace Baldr

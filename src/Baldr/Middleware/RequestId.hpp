#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

#include <Baldr/Middleware/IMiddleware.hpp>

class RequestIdMiddleware final : public IMiddleware
{
  public:
    RequestIdMiddleware() = default;
    ~RequestIdMiddleware() override = default;

    static constexpr const char* kHeaderName = "X-Request-ID";

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        auto it = request.headers.find("x-request-id");
        std::string id;
        if (it != request.headers.end() && !it->second.empty())
        {
            id = it->second;
        }
        else
        {
            id = generate();
        }

        request.headers[kHeaderName] = id;
        response.headers[kHeaderName] = id;

        next();
    }

  private:
    static std::string generate()
    {
        // Lightweight random hex string derived from time + address hash;
        // not cryptographically strong, but stable enough for correlation.
        static thread_local std::uint64_t counter = 0;
        const auto now = static_cast<std::uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        const auto mix = now ^ (++counter * 0x9E3779B97F4A7C15ULL);
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016llx",
                      static_cast<unsigned long long>(mix));
        return buf;
    }
};
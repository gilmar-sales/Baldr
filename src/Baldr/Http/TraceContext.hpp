#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>

namespace BALDR_NAMESPACE
{

    struct TraceContext
    {
        std::uint8_t version { 0 };
        std::string  traceId;
        std::string  spanId;
        std::uint8_t traceFlags { 0 };
        bool         valid { false };

        bool sampled() const noexcept { return (traceFlags & 0x01) != 0; }
    };

    inline bool IsAllZeroHex(std::string_view s) noexcept
    {
        if (s.empty())
        {
            return true;
        }
        for (char c : s)
        {
            if (c != '0')
            {
                return false;
            }
        }
        return true;
    }

    inline bool IsLowerHex(std::string_view s) noexcept
    {
        if (s.empty())
        {
            return false;
        }
        for (char c : s)
        {
            const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            if (!ok)
            {
                return false;
            }
        }
        return true;
    }

    inline std::string NewTraceId() noexcept
    {
        static thread_local std::uint64_t counter = 0;
        const auto                        now     = static_cast<std::uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        const auto mix = now ^ (++counter * 0x9E3779B97F4A7C15ULL);
        char       buf[33];
        std::snprintf(buf, sizeof(buf), "%016llx%016llx",
                      static_cast<unsigned long long>(mix),
                      static_cast<unsigned long long>(now));
        std::string id(buf);
        if (IsAllZeroHex(id))
        {
            id.front() = '1';
        }
        return id;
    }

    inline std::string NewSpanId() noexcept
    {
        static thread_local std::mt19937_64 rng { static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()) };
        const auto                          a = rng();
        char                                buf[17];
        std::snprintf(buf, sizeof(buf), "%016llx",
                      static_cast<unsigned long long>(a));
        std::string id(buf);
        if (IsAllZeroHex(id))
        {
            buf[0]     = '1';
            id         = buf;
            id.front() = '1';
        }
        return id;
    }

    inline bool TryParseTraceparent(std::string_view header,
                                    TraceContext&    out) noexcept
    {
        out = TraceContext {};

        while (!header.empty() &&
               (header.front() == ' ' || header.front() == '\t'))
        {
            header.remove_prefix(1);
        }
        while (!header.empty() &&
               (header.back() == ' ' || header.back() == '\t'))
        {
            header.remove_suffix(1);
        }

        if (header.empty())
        {
            return false;
        }

        constexpr std::size_t kMinFields = 4;
        std::string_view      fields[kMinFields + 1];
        std::size_t           count = 0;

        std::size_t pos = 0;
        while (pos <= header.size() && count < kMinFields + 1)
        {
            const auto dash = header.find('-', pos);
            if (dash == std::string_view::npos)
            {
                fields[count++] = header.substr(pos);
                pos             = header.size() + 1;
                break;
            }
            fields[count++] = header.substr(pos, dash - pos);
            pos             = dash + 1;
        }

        if (count < kMinFields)
        {
            return false;
        }

        const auto& version = fields[0];
        const auto& traceId = fields[1];
        const auto& spanId  = fields[2];
        const auto& flags   = fields[3];

        if (version.size() != 2 || !IsLowerHex(version))
        {
            return false;
        }
        if (traceId.size() != 32 || !IsLowerHex(traceId) ||
            IsAllZeroHex(traceId))
        {
            return false;
        }
        if (spanId.size() != 16 || !IsLowerHex(spanId) || IsAllZeroHex(spanId))
        {
            return false;
        }
        if (flags.size() != 2 || !IsLowerHex(flags))
        {
            return false;
        }

        std::uint32_t ver = 0;
        for (char c : version)
        {
            ver = (ver << 4) |
                  static_cast<std::uint32_t>(
                      c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10);
        }

        std::uint32_t fl = 0;
        for (char c : flags)
        {
            fl = (fl << 4) | static_cast<std::uint32_t>(
                                 c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10);
        }

        out.version    = static_cast<std::uint8_t>(ver);
        out.traceId    = std::string(traceId);
        out.spanId     = std::string(spanId);
        out.traceFlags = static_cast<std::uint8_t>(fl & 0xFF);
        out.valid      = true;
        return true;
    }

} // namespace BALDR_NAMESPACE
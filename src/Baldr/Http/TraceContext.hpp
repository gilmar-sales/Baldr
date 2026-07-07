/**
 * @file Http/TraceContext.hpp
 * @brief W3C Trace Context (version 00) parsing, generation and helpers.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>

#include <Baldr/Hosting/SecureRandom.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Parsed W3C Trace Context state.
     *
     * Carries the version, trace ID, span ID and trace flags. @ref valid
     * indicates whether the values were sourced from a well-formed
     * @c traceparent header.
     */
    struct TraceContext
    {
        std::uint8_t version { 0 };    ///< @c traceparent version byte.
        std::string  traceId;          ///< 32-hex-character trace identifier.
        std::string  spanId;           ///< 16-hex-character span identifier.
        std::uint8_t traceFlags { 0 }; ///< @c traceparent flags byte.
        bool         valid { false }; ///< True when parsed from a valid header.

        /**
         * @brief @c true when the sampled flag (bit 0) is set.
         */
        bool sampled() const noexcept { return (traceFlags & 0x01) != 0; }
    };

    /**
     * @brief @c true when @p s is empty or every character is @c '0'.
     */
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

    /**
     * @brief @c true when @p s is non-empty and every character is a
     *        lowercase hexadecimal digit (0-9, a-f).
     */
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

    /**
     * @brief Generate a new 32-hex-character trace identifier.
     *
     * Backed by the framework's cryptographic RNG (random_device + mt19937_64).
     * If the result is all-zero (1 in 2^128, negligible) the first nibble
     * is forced to @c '1' because all-zero is reserved and rejected by the
     * parser.
     */
    inline std::string NewTraceId() noexcept
    {
        std::string id = RandomHex(32);
        if (IsAllZeroHex(id))
        {
            id.front() = '1';
        }
        return id;
    }

    /**
     * @brief Generate a new 16-hex-character span identifier.
     */
    inline std::string NewSpanId() noexcept
    {
        std::string id = RandomHex(16);
        if (IsAllZeroHex(id))
        {
            id.front() = '1';
        }
        return id;
    }

    /**
     * @brief Parse a @c traceparent header value into @p out.
     *
     * Strictly validates the version, trace-id, span-id and flags fields
     * according to the W3C Trace Context spec (version 00). On failure
     * @p out is reset and the function returns @c false.
     *
     * @param header Raw @c traceparent value.
     * @param out    Receives the parsed context on success.
     * @return @c true on a well-formed header.
     */
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
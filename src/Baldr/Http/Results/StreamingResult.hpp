/**
 * @file Http/Results/StreamingResult.hpp
 * @brief Streaming response interface and chunked-encoding helpers used
 *        for responses whose body is produced lazily.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <trantor/net/TcpConnection.h>

#include <Baldr/Hosting/StringHelpers.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Result type whose body is produced lazily as a sequence of
     *        chunks (typically for SSE, file streaming, or backpressured
     *        producer pipelines).
     *
     * The framework wraps the output in HTTP/1.1 chunked transfer encoding;
     * implementations must not set @c Content-Length or
     * @c Transfer-Encoding themselves.
     */
    class IStreamingResult
    {
      public:
        virtual ~IStreamingResult() = default;

        /**
         * @brief Populate @p out with the response headers to send.
         *
         * @c Content-Type may be set here. The framework overrides
         * @c Transfer-Encoding.
         */
        virtual void headers(
            std::vector<std::pair<std::string, std::string>>& out) const
        {
            (void) out;
        }

        /**
         * @brief Status code of the response. Default 200 OK.
         */
        virtual StatusCode statusCode() const { return StatusCode::OK; }

        /**
         * @brief Pull the next chunk into @p out.
         *
         * Return @c false when no more chunks remain. The chunk is appended
         * to the output buffer; the framework emits the chunked envelope.
         */
        virtual bool nextChunk(std::string& out) const = 0;
    };

    /**
     * @brief @ref IStreamingResult driven by a user-supplied producer callback.
     *
     * Useful for SSE or backpressure-friendly reads from a producer.
     *
     * @code
     * ChunkedStreamResult([&](std::string& out) -> bool {
     *     if (finished) return false;
     *     out = nextEvent();
     *     return true;
     * });
     * @endcode
     */
    class ChunkedStreamResult final : public IStreamingResult
    {
      public:
        /**
         * @brief Callable type invoked to pull each chunk.
         *
         * Implementations should append to @p out and return @c true to
         * continue streaming or @c false to terminate the stream.
         */
        using Producer = std::function<bool(std::string&)>;

        /**
         * @brief Wrap @p producer as a streaming result.
         */
        explicit ChunkedStreamResult(Producer producer) :
            mProducer(std::move(producer))
        {
        }

        bool nextChunk(std::string& out) const override
        {
            if (!mProducer)
                return false;
            out.clear();
            bool more = mProducer(out);
            return more;
        }

      private:
        Producer mProducer;
    };

    /**
     * @brief Format a single chunked-transfer-encoding frame.
     *
     * Produces @c "<hex-size>\r\n<data>\r\n".
     */
    inline std::string formatChunk(std::string_view data)
    {
        static_assert(sizeof(std::size_t) <= sizeof(unsigned long long),
                      "size_t larger than unsigned long long is not "
                      "representable by the chunk-size formatter");
        char        header[32];
        auto        size = data.size();
        int         n    = std::snprintf(header,
                                         sizeof(header),
                                         "%llx\r\n",
                                         static_cast<unsigned long long>(size));
        std::string out;
        out.reserve(n + 2 + size + 2);
        out.append(header, static_cast<std::size_t>(n));
        out.append(data.data(), data.size());
        out.append("\r\n", 2);
        return out;
    }

    /**
     * @brief Format the terminating empty chunk (@c "0\r\n\r\n").
     */
    inline std::string formatChunkTrailer()
    {
        return "0\r\n\r\n";
    }

    /**
     * @brief Assemble the status line, headers and @c Set-Cookie lines for a
     *        streaming response. Adds @c Transfer-Encoding: chunked when not
     *        already supplied.
     *
     * Validates each header name (RFC 9110 token) and each header/cookie
     * value (no CR or LF). If any value fails validation the function
     * returns an empty string and sets @p ok to @c false; callers MUST
     * check @p ok before transmitting the result.
     */
    inline std::string formatStreamingHead(
        StatusCode                                              status,
        const std::string&                                      version,
        const std::vector<std::pair<std::string, std::string>>& headers,
        const std::vector<std::pair<std::string, std::string>>& cookies,
        const std::function<const char*(StatusCode)>&           reasonPhrase,
        bool&                                                   ok)
    {
        ok = true;
        for (const auto& [name, value] : headers)
        {
            if (!isValidHeaderName(name) || containsCrlf(value))
            {
                ok = false;
                return {};
            }
        }
        for (const auto& [name, value] : cookies)
        {
            if (containsCrlf(name) || containsCrlf(value))
            {
                ok = false;
                return {};
            }
        }

        std::string out;
        out.reserve(128);
        out.append(version);
        out.push_back(' ');
        out.append(std::to_string(static_cast<int>(status)));
        out.push_back(' ');
        out.append(reasonPhrase(status));
        out.append("\r\n");

        bool hasTransferEncoding = false;
        for (const auto& [name, value] : headers)
        {
            if (name == "Transfer-Encoding")
                hasTransferEncoding = true;
            out.append(name);
            out.append(": ");
            out.append(value);
            out.append("\r\n");
        }
        if (!hasTransferEncoding)
        {
            out.append("Transfer-Encoding: chunked\r\n");
        }

        for (const auto& [name, value] : cookies)
        {
            out.append("Set-Cookie: ");
            out.append(name);
            out.append("=");
            out.append(value);
            out.append("\r\n");
        }

        out.append("\r\n");
        return out;
    }

} // namespace BALDR_NAMESPACE

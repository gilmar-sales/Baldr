#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <trantor/net/TcpConnection.h>

#include "StatusCode.hpp"

class IStreamingResult
{
  public:
    virtual ~IStreamingResult() = default;

    // Optional response headers. Implementations may also set
    // "Content-Type" here. Implementations MUST NOT set
    // "Content-Length" or "Transfer-Encoding"; the framework sets
    // Transfer-Encoding: chunked automatically.
    virtual void headers(std::vector<std::pair<std::string, std::string>>& out)
        const
    {
        (void)out;
    }

    // Status code of the response. Default 200.
    virtual StatusCode statusCode() const { return StatusCode::OK; }

    // Pull the next chunk. Return false when no more chunks remain. The
    // chunk is appended to the output buffer; the framework is
    // responsible for emitting the chunked encoding envelope.
    // `mutable` state inside the implementation is allowed.
    virtual bool nextChunk(std::string& out) const = 0;
};

// A simple streaming result driven by a user-supplied callback that
// produces chunks lazily. Useful for SSE or backpressure-friendly
// reads from a producer.
//
// Example:
//   ChunkedStreamResult([&](std::string& out) -> bool {
//       if (finished) return false;
//       out = nextEvent();
//       return true;
//   });
class ChunkedStreamResult final : public IStreamingResult
{
  public:
    using Producer = std::function<bool(std::string&)>;

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

// Helper: format a chunked transfer-encoding frame.
inline std::string formatChunk(std::string_view data)
{
    char header[32];
    auto size = data.size();
    int  n    = std::snprintf(header, sizeof(header), "%zx\r\n", size);
    std::string out;
    out.reserve(n + 2 + size + 2);
    out.append(header, static_cast<std::size_t>(n));
    out.append(data.data(), data.size());
    out.append("\r\n", 2);
    return out;
}

inline std::string formatChunkTrailer()
{
    return "0\r\n\r\n";
}

// Helper: assemble the status line + headers for a streaming response.
inline std::string formatStreamingHead(
    StatusCode status,
    const std::string& version,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::vector<std::pair<std::string, std::string>>& cookies,
    const std::function<const char*(StatusCode)>& reasonPhrase)
{
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

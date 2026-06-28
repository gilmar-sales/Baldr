#include "Baldr/StringHelpers.hpp"
#include <algorithm>
#include <cctype>
#include <ranges>
#include <sstream>

#include "Baldr/HttpRequestParser.hpp"
#include "Baldr/StatusCode.hpp"

namespace
{
    std::optional<HttpMethod> parseMethodString(std::string_view method)
    {
        if (method == "GET")
            return HttpMethod::Get;
        if (method == "POST")
            return HttpMethod::Post;
        if (method == "PUT")
            return HttpMethod::Put;
        if (method == "DELETE")
            return HttpMethod::Delete;
        if (method == "PATCH")
            return HttpMethod::Patch;
        if (method == "OPTIONS")
            return HttpMethod::Options;
        if (method == "HEAD")
            return HttpMethod::Head;
        if (method == "TRACE")
            return HttpMethod::Trace;
        if (method == "CONNECT")
            return HttpMethod::Connect;

        return std::nullopt;
    }

    struct ParseResult
    {
        bool            success = false;
        HttpRequest     request;
        std::string     error;
        StatusCode      statusCode = StatusCode::BadRequest;
        std::size_t     consumedBytes = 0;
    };

    // Core parser operating on a contiguous read-only view.
    // `buffer` is the entire accumulated bytes so far; we figure out how
    // many bytes the next complete request consumed and report it via
    // `out.consumedBytes`. Used both for full-buffer tests and incremental
    // server parsing.
    ParseResult parseCore(std::string_view buffer, bool strictHeaders)
    {
        ParseResult out;

        auto headerEnd = buffer.find("\r\n\r\n");
        std::size_t headerByteCount;
        if (headerEnd == std::string_view::npos)
        {
            if (strictHeaders)
            {
                out.error      = "Incomplete request";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }
            headerByteCount = buffer.size();
        }
        else
        {
            headerByteCount = headerEnd + 4;
        }

        std::string_view headerView(buffer.data(), headerByteCount);

        std::istringstream requestStream{std::string(headerView)};

        std::string httpMethod;
        requestStream >> httpMethod;

        auto parsedHttpMethod = parseMethodString(httpMethod);
        if (!parsedHttpMethod.has_value())
        {
            out.error      = "Malformed HTTP method";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        out.request.method = parsedHttpMethod.value();

        requestStream.get();
        if (static_cast<std::size_t>(requestStream.tellg()) < headerView.size() &&
            buffer[static_cast<std::size_t>(requestStream.tellg())] == ' ')
        {
            out.error      = "Extra whitespace in method";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        requestStream >> out.request.path;
        if (out.request.path.size() > 2048)
        {
            out.error      = "Path is too long";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        auto decodedPath = decode_path(out.request.path);
        if (!decodedPath.has_value())
        {
            out.error      = "Invalid URL encoding in path";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        out.request.path = decodedPath.value();

        requestStream.get();
        if (static_cast<std::size_t>(requestStream.tellg()) < headerView.size() &&
            buffer[static_cast<std::size_t>(requestStream.tellg())] == ' ')
        {
            out.error      = "Extra whitespace in path";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        requestStream >> out.request.version;
        if (!out.request.version.starts_with("HTTP/1.1"))
        {
            out.error      = "HTTP version is missing or invalid";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        if (static_cast<std::size_t>(requestStream.tellg()) < headerView.size() &&
            buffer[static_cast<std::size_t>(requestStream.tellg())] == ' ')
        {
            out.error      = "Extra whitespace in version";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        auto queryIndex = out.request.path.find('?');
        if (queryIndex != std::string::npos)
        {
            auto params =
                std::string_view(out.request.path.begin() + queryIndex + 1,
                                 out.request.path.end());
            for (const auto& part : params | std::views::split('&'))
            {
                std::string pair(part.begin(), part.end());
                auto equalsIndex = pair.find('=');
                if (equalsIndex != std::string::npos)
                {
                    auto key   = decode_path(trim(pair.substr(0, equalsIndex)));
                    auto value = decode_path(trim(pair.substr(equalsIndex + 1)));
                    if (!key.has_value() || !value.has_value())
                    {
                        out.error      = "Invalid URL encoding in query";
                        out.statusCode = StatusCode::BadRequest;
                        return out;
                    }
                    out.request.query.emplace(key.value(), value.value());
                }
                else
                {
                    out.request.query[pair] = "";
                }
            }
            out.request.path.resize(queryIndex);
        }

        std::string line;
        while (std::getline(requestStream, line) && requestStream.good())
        {
            if (out.request.headers.size() > 100)
            {
                out.error      = "Too many headers";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            if (auto colon = line.find(':'); colon != std::string::npos)
            {
                auto key = trim(line.substr(0, colon));
                if (key.size() > 64)
                {
                    out.error      = "Header key is too long";
                    out.statusCode = StatusCode::BadRequest;
                    return out;
                }
                std::transform(key.begin(), key.end(), key.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                if (out.request.headers.contains(key))
                {
                    out.error      = "Duplicate headers are not allowed";
                    out.statusCode = StatusCode::BadRequest;
                    return out;
                }

                auto value = trim(line.substr(colon + 1));
                if (value.size() > 4096)
                {
                    out.error      = "Header value is too large";
                    out.statusCode = StatusCode::BadRequest;
                    return out;
                }

                out.request.headers[std::move(key)] = std::move(value);
                continue;
            }

            if (out.request.headers.contains("content-length"))
            {
                // Header block ends here. Body handling is done below.
                break;
            }
            else
            {
                if (!(line == "\r"))
                {
                    out.error      = "Header folding is not allowed in HTTP/1.1";
                    out.statusCode = StatusCode::BadRequest;
                    return out;
                }
            }
        }

        if (line.size() > 0 && out.request.headers.empty())
        {
            out.error      = "Missing end of request line";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        if (out.request.headers.contains("content-length"))
        {
            if (out.request.headers.contains("transfer-encoding"))
            {
                out.error =
                    "Conflicting Content-Length and Transfer-Encoding headers";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            auto contentLength =
                std::atoi(out.request.headers["content-length"].c_str());

            if (contentLength < 0)
            {
                out.error      = "Invalid Content-Length header";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            if (headerByteCount + static_cast<std::size_t>(contentLength) >
                buffer.size())
            {
                out.error =
                    "Request body incomplete; need more bytes from the socket";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            out.request.body.assign(
                buffer.data() + headerByteCount,
                static_cast<std::size_t>(contentLength));
            out.consumedBytes = headerByteCount +
                                static_cast<std::size_t>(contentLength);
        }
        else if (out.request.method == HttpMethod::Post)
        {
            out.error      = "Missing Content-Length header";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        else
        {
            out.consumedBytes = headerByteCount;
        }

        if (!out.request.headers.contains("host"))
        {
            out.error      = "Missing Host header";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        out.success    = true;
        out.statusCode = StatusCode::OK;
        return out;
    }
}  // namespace

HttpResult<HttpRequest> HttpRequestParser::parse(const std::string& request)
{
    auto buffer = std::string_view(request.data(), request.size());
    auto core   = parseCore(buffer, /*strictHeaders=*/false);

    HttpResult<HttpRequest> result;
    result.success    = core.success;
    result.value      = std::move(core.request);
    result.error      = std::move(core.error);
    result.statusCode = core.statusCode;
    return result;
}

HttpResult<HttpRequest> HttpRequestParser::parse(const std::string& request,
                                                 std::size_t        headerByteCount)
{
    auto buffer = std::string_view(request.data(), request.size());
    auto core   = parseCore(buffer, /*strictHeaders=*/false);

    HttpResult<HttpRequest> result;
    result.success    = core.success;
    result.value      = std::move(core.request);
    result.error      = std::move(core.error);
    result.statusCode = core.statusCode;
    (void)headerByteCount;
    return result;
}

HttpParseStatus HttpRequestParser::tryParse(std::string_view buffer) const
{
    HttpParseStatus out;

    auto headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string_view::npos)
    {
        out.kind       = HttpParseStatus::Kind::Incomplete;
        out.statusCode = StatusCode::BadRequest;
        return out;
    }
    std::size_t headerByteCount = headerEnd + 4;

    // The remaining bytes are the body. If Content-Length is declared and we
    // don't yet have that many bytes, we are incomplete.
    auto contentLengthIt =
        [&]() -> std::optional<std::size_t> {
            std::size_t pos = 0;
            while (pos < headerByteCount)
            {
                auto eol = buffer.find("\r\n", pos);
                if (eol == std::string_view::npos || eol > headerByteCount)
                    break;
                std::string_view line(buffer.data() + pos, eol - pos);
                pos = eol + 2;
                auto colon = line.find(':');
                if (colon == std::string_view::npos)
                    continue;
                std::string_view key = line.substr(0, colon);
                // Lowercase compare.
                if (key.size() == 14)
                {
                    bool match = true;
                    for (size_t i = 0; i < 14; ++i)
                    {
                        char c = key[i];
                        if (c >= 'A' && c <= 'Z')
                            c = static_cast<char>(c + 32);
                        static constexpr char expected[] = "content-length";
                        if (c != expected[i])
                        {
                            match = false;
                            break;
                        }
                    }
                    if (match)
                    {
                        std::string value(line.substr(colon + 1));
                        // Trim whitespace.
                        auto begin = value.find_first_not_of(" \t");
                        auto end   = value.find_last_not_of(" \t");
                        if (begin == std::string::npos)
                            return 0;
                        value = value.substr(begin, end - begin + 1);
                        return static_cast<std::size_t>(std::atoll(value.c_str()));
                    }
                }
            }
            return std::nullopt;
        }();

    if (contentLengthIt.has_value())
    {
        std::size_t needed = headerByteCount + *contentLengthIt;
        if (buffer.size() < needed)
        {
            out.kind       = HttpParseStatus::Kind::Incomplete;
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
    }

    auto core = parseCore(buffer, /*strictHeaders=*/true);
    if (!core.success)
    {
        out.kind          = HttpParseStatus::Kind::Error;
        out.errorMessage  = std::move(core.error);
        out.statusCode    = core.statusCode;
        return out;
    }

    out.kind          = HttpParseStatus::Kind::Complete;
    out.request       = std::move(core.request);
    out.consumedBytes = core.consumedBytes ? core.consumedBytes : headerByteCount;
    out.statusCode    = StatusCode::OK;
    return out;
}

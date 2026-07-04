#include <Baldr/Detail/Namespace.hpp>
#include <Baldr/Hosting/StringHelpers.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ranges>
#include <string_view>

#include <Baldr/Http/RequestParser.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    bool containsLiteralPercent00(const std::string& s)
    {
        auto toLower = [](unsigned char c) {
            return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
        };
        for (std::size_t i = 0; i + 2 < s.size(); ++i)
        {
            if (s[i] == '%' &&
                toLower(static_cast<unsigned char>(s[i + 1])) == '0' &&
                toLower(static_cast<unsigned char>(s[i + 2])) == '0')
            {
                return true;
            }
        }
        return false;
    }

    bool isLws(char c)
    {
        return c == ' ' || c == '\t';
    }

    std::size_t skipLws(std::string_view s, std::size_t pos)
    {
        while (pos < s.size() && isLws(s[pos]))
            ++pos;
        return pos;
    }

    // Returns the index just past the final \n of \r\n\r\n, i.e. the byte
    // count of the header section. Returns npos when no complete header
    // terminator is present yet.
    std::size_t findHeaderEnd(std::string_view buffer)
    {
        for (std::size_t i = 0; i + 3 < buffer.size(); ++i)
        {
            if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
                buffer[i + 2] == '\r' && buffer[i + 3] == '\n')
            {
                return i + 4;
            }
        }
        return std::string_view::npos;
    }

    struct ParseResult
    {
        bool        success = false;
        HttpRequest request;
        std::string error;
        StatusCode  statusCode    = StatusCode::BadRequest;
        std::size_t consumedBytes = 0;
    };

    // Token parser operating on `buffer` with explicit offset tracking.
    // On error returns ParseResult with success=false; on success populates
    // `out`.
    ParseResult parseCore(std::string_view buffer, std::size_t maxBodySize)
    {
        ParseResult out;

        auto headerEnd = findHeaderEnd(buffer);
        if (headerEnd == std::string_view::npos)
        {
            out.error      = "Incomplete request";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        std::size_t pos = 0;

        // Request line: METHOD SP PATH SP HTTP/version CRLF
        std::size_t methodStart = pos;
        while (pos < headerEnd && buffer[pos] != ' ' && buffer[pos] != '\r')
            ++pos;
        if (pos == methodStart || pos >= headerEnd || buffer[pos] != ' ')
        {
            out.error      = "Malformed HTTP method";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        std::string_view methodView(
            buffer.data() + methodStart, pos - methodStart);

        std::optional<HttpMethod> parsedMethod;
        if (methodView == "GET")
            parsedMethod = HttpMethod::Get;
        else if (methodView == "POST")
            parsedMethod = HttpMethod::Post;
        else if (methodView == "PUT")
            parsedMethod = HttpMethod::Put;
        else if (methodView == "DELETE")
            parsedMethod = HttpMethod::Delete;
        else if (methodView == "PATCH")
            parsedMethod = HttpMethod::Patch;
        else if (methodView == "OPTIONS")
            parsedMethod = HttpMethod::Options;
        else if (methodView == "HEAD")
            parsedMethod = HttpMethod::Head;
        else if (methodView == "TRACE")
            parsedMethod = HttpMethod::Trace;
        else if (methodView == "CONNECT")
            parsedMethod = HttpMethod::Connect;

        if (!parsedMethod.has_value())
        {
            out.error      = "Malformed HTTP method";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        out.request.method = parsedMethod.value();

        // Reject double-space (extra whitespace) in method position.
        if (pos + 1 < headerEnd && buffer[pos + 1] == ' ')
        {
            out.error      = "Extra whitespace in method";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        ++pos; // consume SP
        std::size_t pathStart = pos;
        while (pos < headerEnd && buffer[pos] != ' ' && buffer[pos] != '\r')
            ++pos;
        if (pos >= headerEnd)
        {
            out.error      = "HTTP version is missing or invalid";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        if (buffer[pos] == '\r')
        {
            out.error      = "HTTP version is missing or invalid";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        if (pos == pathStart || buffer[pos] != ' ')
        {
            out.error      = "Malformed request path";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        std::string_view pathView(buffer.data() + pathStart, pos - pathStart);

        if (pathView.size() > 2048)
        {
            out.error      = "Path is too long";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        auto decodedPath = decode_path(std::string(pathView));
        if (!decodedPath.has_value())
        {
            out.error      = "Invalid URL encoding in path";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        if (containsLiteralPercent00(decodedPath.value()))
        {
            out.error      = "Invalid URL encoding in path";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        out.request.path = decodedPath.value();

        // Reject extra whitespace after path.
        if (pos + 1 < headerEnd && buffer[pos + 1] == ' ')
        {
            out.error      = "Extra whitespace in path";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        ++pos; // consume SP
        std::size_t versionStart = pos;
        while (pos < headerEnd && buffer[pos] != ' ' && buffer[pos] != '\r')
            ++pos;
        if (pos >= headerEnd)
        {
            out.error      = "HTTP version is missing or invalid";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        if (buffer[pos] == ' ')
        {
            out.error      = "Extra whitespace in version";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        if (pos == versionStart || buffer[pos] != '\r' ||
            pos + 1 >= headerEnd || buffer[pos + 1] != '\n')
        {
            out.error      = "HTTP version is missing or invalid";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        std::string_view versionView(buffer.data() + versionStart,
                                     pos - versionStart);
        if (!versionView.starts_with("HTTP/1.1"))
        {
            out.error      = "HTTP version is missing or invalid";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        out.request.version = std::string(versionView);

        pos += 2; // consume CRLF after request line

        // Query string handling.
        auto queryIndex = out.request.path.find('?');
        if (queryIndex != std::string::npos)
        {
            auto params =
                std::string_view(out.request.path.begin() + queryIndex + 1,
                                 out.request.path.end());
            for (const auto& part : params | std::views::split('&'))
            {
                std::string pair(part.begin(), part.end());
                auto        equalsIndex = pair.find('=');
                if (equalsIndex != std::string::npos)
                {
                    auto key = decode_path(trim(pair.substr(0, equalsIndex)));
                    auto value =
                        decode_path(trim(pair.substr(equalsIndex + 1)));
                    if (!key.has_value() || !value.has_value())
                    {
                        out.error      = "Invalid URL encoding in query";
                        out.statusCode = StatusCode::BadRequest;
                        return out;
                    }
                    if (containsLiteralPercent00(key.value()) ||
                        containsLiteralPercent00(value.value()))
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

        // Header lines, bounded by \r\n\r\n at headerEnd.
        std::size_t contentLength       = 0;
        bool        hasContentLength    = false;
        bool        hasTransferEncoding = false;

        while (pos < headerEnd)
        {
            // End-of-headers line: bare "\r\n".
            if (buffer[pos] == '\r' && pos + 1 < headerEnd &&
                buffer[pos + 1] == '\n')
            {
                pos += 2;
                break;
            }

            // Header folding: line begins with SP/HTAB.
            if (isLws(buffer[pos]))
            {
                out.error      = "Header folding is not allowed in HTTP/1.1";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            if (out.request.headers.size() > 100)
            {
                out.error      = "Too many headers";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            std::size_t lineStart = pos;
            while (pos < headerEnd && buffer[pos] != '\r')
                ++pos;
            if (pos >= headerEnd || pos + 1 >= headerEnd ||
                buffer[pos + 1] != '\n')
            {
                out.error      = "Malformed header line";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }
            std::string_view line(buffer.data() + lineStart, pos - lineStart);

            auto colon = line.find(':');
            if (colon == std::string_view::npos)
            {
                out.error      = "Malformed header line";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            std::string key(line.substr(0, colon));
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (key.empty() || key.size() > 64)
            {
                out.error      = "Header key is too long";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }
            if (out.request.headers.contains(key))
            {
                out.error      = "Duplicate headers are not allowed";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            auto        valueSv = line.substr(colon + 1);
            auto        begin   = skipLws(valueSv, 0);
            std::size_t end     = valueSv.size();
            while (end > begin && isLws(valueSv[end - 1]))
                --end;
            std::string value(valueSv.substr(begin, end - begin));

            if (value.size() > 4096)
            {
                out.error      = "Header value is too large";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }

            if (key == "content-length")
            {
                if (value.empty())
                {
                    out.error      = "Invalid Content-Length header";
                    out.statusCode = StatusCode::BadRequest;
                    return out;
                }
                std::size_t parsed = 0;
                for (char c : value)
                {
                    if (c < '0' || c > '9')
                    {
                        out.error      = "Invalid Content-Length header";
                        out.statusCode = StatusCode::BadRequest;
                        return out;
                    }
                    parsed = parsed * 10 + static_cast<std::size_t>(c - '0');
                }
                if (parsed == 0)
                {
                    out.error      = "Invalid Content-Length header";
                    out.statusCode = StatusCode::BadRequest;
                    return out;
                }
                if (parsed > maxBodySize)
                {
                    out.error =
                        "Content-Length exceeds maximum allowed body size";
                    out.statusCode = StatusCode::BadRequest;
                    return out;
                }
                contentLength    = parsed;
                hasContentLength = true;
            }
            else if (key == "transfer-encoding")
            {
                hasTransferEncoding = true;
            }

            out.request.headers.emplace(std::move(key), std::move(value));

            pos += 2; // consume CRLF
        }

        if (hasContentLength && hasTransferEncoding)
        {
            out.error =
                "Conflicting Content-Length and Transfer-Encoding headers";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        if (hasTransferEncoding)
        {
            out.error      = "Transfer-Encoding is not supported";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        if (hasContentLength)
        {
            if (headerEnd + contentLength > buffer.size())
            {
                out.error =
                    "Request body incomplete; need more bytes from the socket";
                out.statusCode = StatusCode::BadRequest;
                return out;
            }
            out.request.body.assign(buffer.data() + headerEnd, contentLength);
            out.consumedBytes = headerEnd + contentLength;
        }
        else if (out.request.method == HttpMethod::Post ||
                 out.request.method == HttpMethod::Put ||
                 out.request.method == HttpMethod::Patch)
        {
            out.error      = "Missing Content-Length header";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }
        else
        {
            out.consumedBytes = headerEnd;
        }

        if (!out.request.headers.contains("host"))
        {
            out.error      = "Missing Host header";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        auto cookieIt = out.request.headers.find("cookie");
        if (cookieIt != out.request.headers.end())
        {
            std::size_t pos = 0;
            const auto& raw = cookieIt->second;
            while (pos < raw.size())
            {
                std::size_t end = raw.find(';', pos);
                if (end == std::string::npos)
                    end = raw.size();
                std::string_view part(raw.data() + pos, end - pos);
                auto             begin = part.find_first_not_of(' ');
                if (begin != std::string_view::npos)
                {
                    auto eq = part.find('=', begin);
                    if (eq != std::string_view::npos && eq != part.size() - 1)
                    {
                        std::string name(part.substr(begin, eq - begin));
                        std::string value(part.substr(eq + 1));
                        out.request.cookies[std::move(name)] = std::move(value);
                    }
                }
                pos = end + 1;
            }
        }

        out.success    = true;
        out.statusCode = StatusCode::OK;
        return out;
    }

    HttpParseStatus HttpRequestParser::tryParse(std::string_view buffer) const
    {
        HttpParseStatus out;

        auto headerEnd = findHeaderEnd(buffer);
        if (headerEnd == std::string_view::npos)
        {
            out.kind       = HttpParseStatus::Kind::Incomplete;
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        // Quick Content-Length peek to determine whether we have enough body
        // bytes for an Incomplete result before running the full parser.
        enum class ContentLengthLookup
        {
            Absent,
            Valid,
            Invalid,
            TooLarge,
        };
        struct ContentLengthResult
        {
            ContentLengthLookup kind  = ContentLengthLookup::Absent;
            std::size_t         value = 0;
        };
        auto contentLengthResult = [&]() -> ContentLengthResult {
            ContentLengthResult result;
            std::size_t         pos = 0;
            while (pos < headerEnd)
            {
                auto eol = buffer.find("\r\n", pos);
                if (eol == std::string_view::npos || eol > headerEnd)
                    break;
                std::string_view line(buffer.data() + pos, eol - pos);
                pos        = eol + 2;
                auto colon = line.find(':');
                if (colon == std::string_view::npos)
                    continue;
                std::string_view key = line.substr(0, colon);
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
                        std::string_view value = line.substr(colon + 1);
                        auto             begin = skipLws(value, 0);
                        std::size_t      end   = value.size();
                        while (end > begin && isLws(value[end - 1]))
                            --end;
                        if (begin >= end)
                        {
                            result.kind  = ContentLengthLookup::Invalid;
                            result.value = 0;
                            return result;
                        }
                        value = value.substr(begin, end - begin);

                        std::size_t parsed = 0;
                        for (char c : value)
                        {
                            if (c < '0' || c > '9')
                            {
                                result.kind  = ContentLengthLookup::Invalid;
                                result.value = 0;
                                return result;
                            }
                            parsed =
                                parsed * 10 + static_cast<std::size_t>(c - '0');
                        }

                        if (parsed == 0)
                        {
                            result.kind  = ContentLengthLookup::Invalid;
                            result.value = 0;
                            return result;
                        }

                        result.kind  = (parsed > maxBodySize)
                                           ? ContentLengthLookup::TooLarge
                                           : ContentLengthLookup::Valid;
                        result.value = parsed;
                        return result;
                    }
                }
            }
            return result;
        }();

        if (contentLengthResult.kind == ContentLengthLookup::Invalid)
        {
            out.kind         = HttpParseStatus::Kind::Error;
            out.errorMessage = "Invalid Content-Length header";
            out.statusCode   = StatusCode::BadRequest;
            return out;
        }
        if (contentLengthResult.kind == ContentLengthLookup::TooLarge)
        {
            out.kind = HttpParseStatus::Kind::Error;
            out.errorMessage =
                "Content-Length exceeds maximum allowed body size";
            out.statusCode = StatusCode::BadRequest;
            return out;
        }

        if (contentLengthResult.kind == ContentLengthLookup::Valid)
        {
            std::size_t needed = headerEnd + contentLengthResult.value;
            if (buffer.size() < needed)
            {
                out.kind       = HttpParseStatus::Kind::Incomplete;
                out.statusCode = StatusCode::BadRequest;
                return out;
            }
        }

        auto core = parseCore(buffer, maxBodySize);
        if (!core.success)
        {
            out.kind         = HttpParseStatus::Kind::Error;
            out.errorMessage = std::move(core.error);
            out.statusCode   = core.statusCode;
            return out;
        }

        out.kind          = HttpParseStatus::Kind::Complete;
        out.request       = std::move(core.request);
        out.consumedBytes = core.consumedBytes ? core.consumedBytes : headerEnd;
        out.statusCode    = StatusCode::OK;
        return out;
    }

} // namespace BALDR_NAMESPACE
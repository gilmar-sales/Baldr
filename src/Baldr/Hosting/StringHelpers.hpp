/**
 * @file Hosting/StringHelpers.hpp
 * @brief Small string utilities used by the HTTP parser (trim, percent
 *        decoding of path/query segments).
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Trim leading and trailing ASCII whitespace from @p str.
     *
     * Recognises space, tab, newline, carriage return, form feed, and
     * vertical tab. Returns the empty string when @p str contains only
     * whitespace.
     *
     * @param str Input string.
     * @return The trimmed substring.
     */
    inline std::string trim(const std::string& str)
    {
        const size_t start = str.find_first_not_of(" \t\n\r\f\v");

        const size_t end = str.find_last_not_of(" \t\n\r\f\v");

        if (start == std::string::npos || end == std::string::npos)
        {
            return "";
        }

        return str.substr(start, end - start + 1);
    }

    /**
     * @brief Percent-decode a URL path segment.
     *
     * Decodes @c %XX sequences (upper- or lower-case hex). Returns
     * @c std::nullopt on a malformed escape or an embedded NUL byte;
     * both indicate a path that should be rejected.
     *
     * @param path Encoded path segment.
     * @return Decoded segment, or @c std::nullopt on failure.
     */
    inline std::optional<std::string> decode_path(std::string_view path)
    {
        auto hexNibble = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        };

        std::string decoded;
        decoded.reserve(path.size());
        for (size_t i = 0; i < path.size(); ++i)
        {
            if (path[i] == '%')
            {
                if (i + 2 >= path.size())
                    return std::nullopt;
                const int hi = hexNibble(path[i + 1]);
                const int lo = hexNibble(path[i + 2]);
                if (hi < 0 || lo < 0)
                    return std::nullopt;
                const char c = static_cast<char>((hi << 4) | lo);
                if (c == '\0')
                    return std::nullopt;
                decoded.push_back(c);
                i += 2;
            }
            else
            {
                decoded.push_back(path[i]);
            }
        }
        return decoded;
    }

    /**
     * @brief @c true when @p s contains a CR or LF byte.
     *
     * Used to reject values that would inject extra header lines into
     * outgoing HTTP responses (HTTP response splitting / CWE-93).
     */
    inline bool containsCrlf(std::string_view s) noexcept
    {
        for (char c : s)
        {
            if (c == '\r' || c == '\n')
                return true;
        }
        return false;
    }

    /**
     * @brief Return an ASCII-lowercased copy of @p s.
     *
     * Non-ASCII bytes pass through unchanged. Used for case-insensitive
     * comparison of HTTP header names, ETag values, and connection tokens.
     */
    inline std::string toLowerAscii(std::string_view s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c >= 'A' && c <= 'Z')
                out.push_back(static_cast<char>(c + 32));
            else
                out.push_back(c);
        }
        return out;
    }

    /**
     * @brief Validate an outgoing HTTP header name per RFC 9110 token rules.
     *
     * Rejects empty names, names containing CR/LF, separators, and other
     * characters that are illegal in field-names. Used by the response
     * writer to prevent header injection.
     */
    inline bool isValidHeaderName(std::string_view s) noexcept
    {
        if (s.empty() || s.size() > 64)
            return false;
        for (char c : s)
        {
            const bool ok =
                (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') || c == '!' || c == '#' || c == '$' ||
                c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' ||
                c == '-' || c == '.' || c == '^' || c == '_' || c == '`' ||
                c == '|' || c == '~';
            if (!ok)
                return false;
        }
        return true;
    }

} // namespace BALDR_NAMESPACE
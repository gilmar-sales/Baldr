/**
 * @file Http/RequestParser.hpp
 * @brief Incremental HTTP/1.1 request parser used by @c HttpConnection.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>
#include <string_view>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Results/HttpResult.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Outcome of a single @ref HttpRequestParser::tryParse call.
     */
    struct HttpParseStatus
    {
        /**
         * @brief Parsing outcome category.
         */
        enum class Kind
        {
            Incomplete, ///< Buffer holds a partial request; need more bytes.
            Complete,   ///< A full request has been parsed successfully.
            Error,      ///< The buffer is malformed; see @c errorMessage.
        };

        Kind        kind = Kind::Incomplete; ///< Outcome category.
        HttpRequest request; ///< Parsed request (populated on @c Complete).
        std::string
            errorMessage; ///< Human-readable error (populated on @c Error).
        StatusCode statusCode =
            StatusCode::OK; ///< Suggested status code on @c Error.
        std::size_t consumedBytes =
            0; ///< Bytes consumed from the buffer on @c Complete.
    };

    /**
     * @brief Stateful, re-entrant HTTP/1.1 request parser.
     *
     * Designed for use on a streaming socket: feed the accumulated receive
     * buffer into @ref tryParse, advance by @c consumedBytes, and repeat
     * until a complete request is available. The parser enforces a
     * Content-Length cap via @c maxBodySize.
     */
    class HttpRequestParser
    {
      public:
        HttpRequestParser() = default;

        /// Maximum allowed Content-Length / body size in bytes.
        std::size_t maxBodySize = 100 * 1024 * 1024;

        /**
         * @brief Attempt to extract one complete request from @p buffer.
         *
         * @param buffer Bytes received from the client (may contain more than
         *               one request).
         * @return An @ref HttpParseStatus describing the outcome.
         */
        HttpParseStatus tryParse(std::string_view buffer) const;
    };

} // namespace BALDR_NAMESPACE

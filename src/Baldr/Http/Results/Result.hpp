/**
 * @file Http/Results/Result.hpp
 * @brief Built-in @ref IResult implementations returned by handlers.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/StatusCode.hpp>

#include <string>
#include <utility>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Polymorphic response object returned by route handlers.
     *
     * Implementations populate an @ref HttpResponse in @ref Apply; the
     * @c WebApplication::MapRoute plumbing stores the result, calls
     * @ref Apply and writes the response. The three accessors
     * (@ref StatusFor, @ref ContentTypeFor, @ref SchemaJsonFor) let the
     * OpenAPI generator render a faithful schema and media type for any
     * result, including the legacy subclasses below that do not derive
     * from @ref TypedResult.
     */
    class IResult
    {
      public:
        virtual ~IResult() = default;

        /**
         * @brief Mutate @p response to reflect this result.
         */
        virtual void Apply(HttpResponse& response) const = 0;

        /**
         * @brief HTTP status code that this result writes to the response.
         */
        [[nodiscard]] virtual StatusCode StatusFor() const = 0;

        /**
         * @brief Content-Type header value the result applies to the
         *        response, or empty when no body is written.
         */
        [[nodiscard]] virtual std::string_view ContentTypeFor() const = 0;

        /**
         * @brief Raw JSON Schema fragment describing the response body.
         *
         * Defaults to @c {"type":"string"} for results that carry a body
         * under a media type, and to @c {} for body-less results.
         */
        [[nodiscard]] virtual std::string_view SchemaJsonFor() const
        {
            return ContentTypeFor().empty()
                       ? std::string_view("{}")
                       : std::string_view("{\"type\":\"string\"}");
        }
    };

    /**
     * @brief @c text/plain response with an explicit status code.
     */
    class TextResult final : public IResult
    {
      public:
        /**
         * @brief Default-construct a text/plain 200 OK response with an
         *        empty body. Used by the OpenAPI generator to introspect
         *        status and content type without an instance.
         */
        TextResult() : mStatus(StatusCode::OK) {}

        /**
         * @brief Construct a text/plain response.
         *
         * @param body   Body string (moved into the result).
         * @param status HTTP status code, defaults to 200 OK.
         */
        TextResult(std::string body, StatusCode status = StatusCode::OK) :
            mBody(std::move(body)), mStatus(status)
        {
        }

        /// @copydoc IResult::Apply
        void Apply(HttpResponse& response) const override
        {
            response.body                    = mBody;
            response.statusCode              = mStatus;
            response.headers["Content-Type"] = "text/plain";
        }

        /// @copydoc IResult::StatusFor
        [[nodiscard]] StatusCode StatusFor() const override { return mStatus; }
        /// @copydoc IResult::ContentTypeFor
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }

      private:
        std::string mBody;
        StatusCode  mStatus;
    };

    /**
     * @brief Status-only response (no body, no headers).
     */
    class StatusResult final : public IResult
    {
      public:
        /**
         * @brief Default-construct a 200 OK status-only response. Used by
         * the OpenAPI generator to introspect status without an instance.
         */
        StatusResult() : mStatus(StatusCode::OK) {}

        /// @brief Construct a status-only response.
        explicit StatusResult(StatusCode status) : mStatus(status) {}

        /// @copydoc IResult::Apply
        void Apply(HttpResponse& response) const override
        {
            response.statusCode = mStatus;
        }

        /// @copydoc IResult::StatusFor
        [[nodiscard]] StatusCode StatusFor() const override { return mStatus; }
        /// @copydoc IResult::ContentTypeFor
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return {};
        }
        /// @copydoc IResult::SchemaJsonFor
        [[nodiscard]] std::string_view SchemaJsonFor() const override
        {
            return "{}";
        }

      private:
        StatusCode mStatus;
    };

    /**
     * @brief Generic body response with an explicit Content-Type.
     */
    class ContentResult final : public IResult
    {
      public:
        /**
         * @brief Default-construct a 200 OK response with empty body and
         * empty content type. Used by the OpenAPI generator to introspect
         * status and content type without an instance.
         */
        ContentResult() : mStatus(StatusCode::OK) {}

        /**
         * @brief Construct a response with an arbitrary Content-Type.
         *
         * @param body        Body string.
         * @param contentType Value for the @c Content-Type header.
         * @param status      HTTP status code, defaults to 200 OK.
         */
        ContentResult(std::string body, std::string contentType,
                      StatusCode status = StatusCode::OK) :
            mBody(std::move(body)), mContentType(std::move(contentType)),
            mStatus(status)
        {
        }

        /// @copydoc IResult::Apply
        void Apply(HttpResponse& response) const override
        {
            response.body                    = mBody;
            response.statusCode              = mStatus;
            response.headers["Content-Type"] = mContentType;
        }

        /// @copydoc IResult::StatusFor
        [[nodiscard]] StatusCode StatusFor() const override { return mStatus; }
        /// @copydoc IResult::ContentTypeFor
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return mContentType;
        }

      private:
        std::string mBody;
        std::string mContentType;
        StatusCode  mStatus;
    };

} // namespace BALDR_NAMESPACE

#include <Baldr/Http/Results/TypedResults.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Convenience factory functions for the most common @ref IResult
     *        shapes.
     *
     * Existing factories (@ref Ok, @ref Json, @ref NotFound, @ref Status)
     * preserve their previous return types for back-compat. The new typed
     * factories (@ref NotFound, @ref Created, @ref NoContent, @ref
     * BadRequest, @ref Unauthorized, @ref Forbidden, @ref Conflict,
     * @ref UnprocessableEntity, @ref InternalServerError) return the matching
     * @ref TypedResult subclass so the OpenAPI generator can emit a separate
     * entry per status code in the operation's @c responses map.
     */
    class Results
    {
      public:
        /**
         * @brief 200 OK text response (back-compat: returns @ref TextResult).
         */
        static TextResult Ok(std::string body)
        {
            return TextResult(std::move(body), StatusCode::OK);
        }

        /**
         * @brief Typed JSON response carrying a structured payload @c T under
         *        HTTP status code @c Status (e.g. @c StatusCode::BadRequest).
         *
         * Returns a @ref JsonResult so the OpenAPI generator emits a
         * @c $ref to the registered schema for @c T under status @c Status
         * instead of the generic @c {"type":"string"} placeholder. The
         * @p reg argument is currently informational; the schema is
         * registered lazily by @c RouteRegistration during route binding.
         *
         * @tparam T      Reflectable struct (or @c std::vector of one).
         * @tparam Status HTTP status code this result writes.
         */
        template <typename T, StatusCode Status>
        static JsonResult<T, Status> Json(const T&        value,
                                          SchemaRegistry* reg = nullptr)
        {
            (void) reg;
            return JsonResult<T, Status>(value);
        }

        /**
         * @brief Status-only response (back-compat: returns @ref StatusResult).
         */
        static StatusResult Status(StatusCode status)
        {
            return StatusResult(status);
        }

        /**
         * @brief 404 Not Found text response.
         *
         * Returns a @ref NotFoundResult so the OpenAPI generator emits a
         * @c 404 entry in the operation's @c responses map.
         */
        static NotFoundResult NotFound(std::string body = "Not Found")
        {
            return NotFoundResult(std::move(body));
        }

        /**
         * @brief 201 Created response with a text body.
         */
        static CreatedResult Created(std::string body = "Created")
        {
            return CreatedResult(std::move(body));
        }

        /**
         * @brief 204 No Content response.
         */
        static NoContentResult NoContent() { return NoContentResult(); }

        /**
         * @brief 400 Bad Request text response.
         */
        static BadRequestResult BadRequest(std::string body = "Bad Request")
        {
            return BadRequestResult(std::move(body));
        }

        /**
         * @brief 401 Unauthorized text response.
         */
        static UnauthorizedResult Unauthorized(
            std::string body = "Unauthorized")
        {
            return UnauthorizedResult(std::move(body));
        }

        /**
         * @brief 403 Forbidden text response.
         */
        static ForbiddenResult Forbidden(std::string body = "Forbidden")
        {
            return ForbiddenResult(std::move(body));
        }

        /**
         * @brief 409 Conflict text response.
         */
        static ConflictResult Conflict(std::string body = "Conflict")
        {
            return ConflictResult(std::move(body));
        }

        /**
         * @brief 422 Unprocessable Entity text response.
         */
        static UnprocessableEntityResult UnprocessableEntity(
            std::string body = "Unprocessable Entity")
        {
            return UnprocessableEntityResult(std::move(body));
        }

        /**
         * @brief 500 Internal Server Error text response.
         */
        static InternalServerErrorResult InternalServerError(
            std::string body = "Internal Server Error")
        {
            return InternalServerErrorResult(std::move(body));
        }
    };

} // namespace BALDR_NAMESPACE
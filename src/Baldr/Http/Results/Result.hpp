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
     * @ref Apply and writes the response.
     */
    class IResult
    {
      public:
        virtual ~IResult() = default;

        /**
         * @brief Mutate @p response to reflect this result.
         */
        virtual void Apply(HttpResponse& response) const = 0;
    };

    /**
     * @brief @c text/plain response with an explicit status code.
     */
    class TextResult final : public IResult
    {
      public:
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

        void Apply(HttpResponse& response) const override
        {
            response.body                    = mBody;
            response.statusCode              = mStatus;
            response.headers["Content-Type"] = "text/plain";
        }

      private:
        std::string mBody;
        StatusCode  mStatus;
    };

    /**
     * @brief @c application/json response carrying a pre-serialised body.
     *
     * The constructor does not serialise; the framework's @c Json helper or
     * the user is expected to call @c simdjson::to_json_string beforehand.
     */
    class JsonResult final : public IResult
    {
      public:
        /**
         * @brief Construct a JSON response from a pre-serialised string.
         *
         * @param body   JSON body.
         * @param status HTTP status code, defaults to 200 OK.
         */
        JsonResult(std::string body, StatusCode status = StatusCode::OK) :
            mBody(std::move(body)), mStatus(status)
        {
        }

        void Apply(HttpResponse& response) const override
        {
            response.body                    = mBody;
            response.statusCode              = mStatus;
            response.headers["Content-Type"] = "application/json";
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
        /// @brief Construct a status-only response.
        explicit StatusResult(StatusCode status) : mStatus(status) {}

        void Apply(HttpResponse& response) const override
        {
            response.statusCode = mStatus;
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

        void Apply(HttpResponse& response) const override
        {
            response.body                    = mBody;
            response.statusCode              = mStatus;
            response.headers["Content-Type"] = mContentType;
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
         * @brief 200 OK JSON response (back-compat: returns @ref JsonResult).
         * Serialises @p value using simdjson.
         */
        template <typename T>
        static JsonResult Json(const T& value)
        {
            return JsonResult(simdjson::to_json_string(value), StatusCode::OK);
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
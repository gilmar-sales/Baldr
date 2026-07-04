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

namespace BALDR_NAMESPACE {

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
    virtual ~IResult()                               = default;

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

/**
 * @brief Convenience factory functions for the most common @ref IResult
 *        shapes.
 */
class Results
{
  public:
    /**
     * @brief 200 OK text response.
     */
    static TextResult Ok(std::string body)
    {
        return TextResult(std::move(body), StatusCode::OK);
    }

    /**
     * @brief 200 OK JSON response. Serialises @p value using simdjson.
     */
    template <typename T>
    static JsonResult Json(const T& value)
    {
        return JsonResult(simdjson::to_json_string(value), StatusCode::OK);
    }

    /**
     * @brief Status-only response.
     */
    static StatusResult Status(StatusCode status)
    {
        return StatusResult(status);
    }

    /**
     * @brief 404 Not Found text response.
     */
    static TextResult NotFound(std::string body = "Not Found")
    {
        return TextResult(std::move(body), StatusCode::NotFound);
    }
};

} // namespace BALDR_NAMESPACE
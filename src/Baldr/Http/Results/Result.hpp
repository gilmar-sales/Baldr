#pragma once

#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/StatusCode.hpp>

#include <string>
#include <utility>

class IResult
{
  public:
    virtual ~IResult() = default;
    virtual void Apply(HttpResponse& response) const = 0;
};

class TextResult final : public IResult
{
  public:
    TextResult(std::string body, StatusCode status = StatusCode::OK) :
        mBody(std::move(body)), mStatus(status)
    {
    }

    void Apply(HttpResponse& response) const override
    {
        response.body             = mBody;
        response.statusCode       = mStatus;
        response.headers["Content-Type"] = "text/plain";
    }

  private:
    std::string mBody;
    StatusCode  mStatus;
};

class JsonResult final : public IResult
{
  public:
    JsonResult(std::string body, StatusCode status = StatusCode::OK) :
        mBody(std::move(body)), mStatus(status)
    {
    }

    void Apply(HttpResponse& response) const override
    {
        response.body             = mBody;
        response.statusCode       = mStatus;
        response.headers["Content-Type"] = "application/json";
    }

  private:
    std::string mBody;
    StatusCode  mStatus;
};

class StatusResult final : public IResult
{
  public:
    explicit StatusResult(StatusCode status) : mStatus(status) {}

    void Apply(HttpResponse& response) const override
    {
        response.statusCode = mStatus;
    }

  private:
    StatusCode mStatus;
};

class ContentResult final : public IResult
{
  public:
    ContentResult(std::string body, std::string contentType,
                  StatusCode status = StatusCode::OK) :
        mBody(std::move(body)),
        mContentType(std::move(contentType)), mStatus(status)
    {
    }

    void Apply(HttpResponse& response) const override
    {
        response.body             = mBody;
        response.statusCode       = mStatus;
        response.headers["Content-Type"] = mContentType;
    }

  private:
    std::string mBody;
    std::string mContentType;
    StatusCode  mStatus;
};

class Results
{
  public:
    static TextResult Ok(std::string body)
    {
        return TextResult(std::move(body), StatusCode::OK);
    }

    template <typename T>
    static JsonResult Json(const T& value)
    {
        return JsonResult(simdjson::to_json_string(value), StatusCode::OK);
    }

    static StatusResult Status(StatusCode status)
    {
        return StatusResult(status);
    }

    static TextResult NotFound(std::string body = "Not Found")
    {
        return TextResult(std::move(body), StatusCode::NotFound);
    }
};
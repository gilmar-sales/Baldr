#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <string>
#include <unordered_set>

#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE {

struct CorsOptions
{
    std::string                     allowOrigin  = "*";
    std::unordered_set<std::string> allowMethods = {
        "GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS"
    };
    std::unordered_set<std::string> allowHeaders     = { "Content-Type",
                                                         "Authorization" };
    bool                            allowCredentials = false;
    int                             maxAge           = 86400;
};

class CorsMiddleware final : public IMiddleware
{
  public:
    explicit CorsMiddleware(CorsOptions options = {}) :
        mOptions(std::move(options))
    {
    }

    ~CorsMiddleware() override = default;

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        response.headers["Access-Control-Allow-Origin"] = mOptions.allowOrigin;
        response.headers["Access-Control-Allow-Methods"] =
            join(mOptions.allowMethods, ", ");
        response.headers["Access-Control-Allow-Headers"] =
            join(mOptions.allowHeaders, ", ");
        response.headers["Access-Control-Max-Age"] =
            std::to_string(mOptions.maxAge);
        if (mOptions.allowCredentials)
            response.headers["Access-Control-Allow-Credentials"] = "true";

        if (request.method == HttpMethod::Options)
        {
            response.statusCode = StatusCode::NoContent;
            return;
        }

        next();
    }

  private:
    static std::string join(const std::unordered_set<std::string>& items,
                            const std::string&                     sep)
    {
        std::string out;
        for (const auto& item : items)
        {
            if (!out.empty())
                out += sep;
            out += item;
        }
        return out;
    }

    CorsOptions mOptions;
};

} // namespace BALDR_NAMESPACE
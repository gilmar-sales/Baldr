#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <Skirnir/Skirnir.hpp>

#include "HttpMethod.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Result.hpp"
#include "RouteOptions.hpp"
#include "Tuple.hpp"

using RouteHandler = std::function<void(
    HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>)>;

struct RouteEntry
{
    std::regex               extractParamsRegex;
    std::vector<std::string> paramsNames = {};
    bool                     hasParams   = true;
    RouteHandler             handler;
    Baldr::RouteOptions      options;
    std::string              groupPrefix;
    std::string              pathTemplate;
    HttpMethod               method { HttpMethod::Get };

    std::unordered_map<std::string, std::string> extractRouteParams(
        const std::string& path) const
    {
        std::unordered_map<std::string, std::string> params;

        if (!hasParams)
        {
            return params;
        }

        std::smatch match;

        if (std::regex_match(path, match, extractParamsRegex))
        {
            for (size_t i = 0; i < paramsNames.size(); ++i)
            {
                params[paramsNames[i]] = match[i + 1];
            }
        }

        return params;
    }
};

struct TrieNode
{
    std::unordered_map<std::string, std::unique_ptr<TrieNode>> children;
    std::optional<RouteEntry>                                  routeEntry;
    bool isEndOfPath = false;

    TrieNode()                               = default;
    TrieNode(TrieNode&&) noexcept            = default;
    TrieNode& operator=(TrieNode&&) noexcept = default;
    TrieNode(const TrieNode&)                = delete;
    TrieNode& operator=(const TrieNode&)     = delete;

    ~TrieNode() = default;
};

class Router
{
  public:
    Router()
    {
        mMethodsMap[HttpMethod::Get]     = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Post]    = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Put]     = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Delete]  = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Patch]   = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Options] = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Head]    = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Trace]   = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Connect] = std::make_unique<TrieNode>();
    }

    ~Router()                            = default;
    Router(const Router&)                = delete;
    Router& operator=(const Router&)     = delete;
    Router(Router&&) noexcept            = default;
    Router& operator=(Router&&) noexcept = default;

    template <typename Handler>
    void MapRoute(HttpMethod method, const std::string& route,
                  const std::string&         groupPrefix,
                  const Baldr::RouteOptions& options, Handler&& handler)
    {
        insert(
            method,
            route,
            options,
            groupPrefix,
            [handler = std::forward<Handler>(handler)](
                HttpRequest&                          request,
                HttpResponse&                         response,
                const skr::Arc<skr::ServiceProvider>& serviceProvider) mutable {
                using HandlerArgsTuple = typename LambdaTraits<
                    std::remove_reference_t<decltype(handler)>>::ArgsTuple;

                auto refFactory = [&]<typename TArg>(TArg* x) -> TArg& {
                    if constexpr (std::is_same_v<HttpRequest, TArg>)
                    {
                        return request;
                    }

                    if constexpr (std::is_same_v<HttpResponse, TArg>)
                    {
                        return response;
                    }

                    return *serviceProvider->GetService<TArg>();
                };

                auto ptrFactory =
                    [&]<typename TArg>(skr::Arc<TArg>* x) -> skr::Arc<TArg> {
                    return serviceProvider
                        ->GetService<std::remove_pointer_t<TArg>>();
                };

                auto args = transformTuple<HandlerArgsTuple>(
                    refFactory, ptrFactory,
                    TupleOfPtr((HandlerArgsTuple*) nullptr));

                using ResultType = LambdaResult<decltype(handler)>;
                if constexpr (!std::is_same_v<ResultType, void>)
                {
                    auto result = std::move(std::apply(handler, args));

                    if constexpr (std::is_base_of_v<IStreamingResult,
                                                    ResultType> &&
                                  !std::is_base_of_v<IResult, ResultType>)
                    {
                        response.streaming =
                            std::make_shared<ResultType>(std::move(result));
                    }
                    else if constexpr (std::is_base_of_v<IResult, ResultType>)
                    {
                        result.Apply(response);
                    }
                    else if constexpr (std::is_same_v<const char*,
                                                      ResultType> ||
                                       std::is_same_v<char*, ResultType>)
                    {
                        response.headers["Content-Type"] = "text/plain";
                        response.body                    = std::string(result);
                        response.statusCode              = StatusCode::OK;
                    }
                    else if constexpr (std::is_assignable_v<std::string,
                                                            ResultType>)
                    {
                        response.headers["Content-Type"] = "text/plain";
                        response.body                    = result;
                        response.statusCode              = StatusCode::OK;
                    }
                    else
                    {
                        simdjson::simdjson_result<std::string> json =
                            simdjson::to_json(result);
                        if (json.has_value())
                        {
                            response.body = std::move(json).take_value();
                            response.headers["Content-Type"] =
                                "application/json";
                            response.statusCode = StatusCode::OK;
                        }
                        else
                        {
                            response.headers["Content-Type"] = "text/plain";
                            response.body =
                                "Handler returned a value that could not be "
                                "serialized to JSON or std::string.";
                            response.statusCode =
                                StatusCode::InternalServerError;
                        }
                    }
                }
                else
                {
                    std::apply(handler, args);
                }
            });
    }

    void insert(HttpMethod method, std::string path,
                const RouteHandler& routeHandler) const;

    void insert(HttpMethod method, std::string path,
                Baldr::RouteOptions options, std::string groupPrefix,
                const RouteHandler& routeHandler) const;

    [[nodiscard]] std::optional<RouteEntry> match(HttpMethod,
                                                  std::string path) const;

    struct MatchResult
    {
        std::optional<RouteEntry> entry;
        std::vector<HttpMethod>   allowedMethodsOnPath;
        HttpMethod                resolvedMethod = HttpMethod::Get;
        std::string               routeTemplate;
    };

    [[nodiscard]] MatchResult matchWithAllow(HttpMethod  method,
                                             std::string path) const;

    // Returns a copy of every registered route in registration order.
    // Used by introspection consumers (e.g. OpenAPI extension).
    std::vector<RouteEntry> Snapshot() const;

  private:
    [[nodiscard]] std::optional<RouteEntry> matchInTrie(
        HttpMethod method, std::string_view path) const;

    [[nodiscard]] std::optional<RouteEntry> matchInTrieWithTemplate(
        HttpMethod method, std::string_view path,
        std::string& outTemplate) const;

    mutable std::shared_mutex                       mMutex {};
    std::map<HttpMethod, std::unique_ptr<TrieNode>> mMethodsMap;
};

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <Skirnir/DependencyInjection/ServiceProvider.hpp>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using RouteHandler = std::function<void(
    HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>)>;

struct RouteEntry
{
    std::regex               extractParamsRegex;
    std::vector<std::string> paramsNames = {};
    bool                     hasParams   = true;
    RouteHandler             handler;

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
    std::optional<RouteEntry>                                 routeEntry;
    bool                                                      isEndOfPath = false;

    TrieNode() = default;
    TrieNode(TrieNode&&) noexcept = default;
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
        mMethodsMap[HttpMethod::Get]      = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Post]     = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Put]      = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Delete]   = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Patch]    = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Options]  = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Head]     = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Trace]    = std::make_unique<TrieNode>();
        mMethodsMap[HttpMethod::Connect]  = std::make_unique<TrieNode>();
    }

    ~Router() = default;
    Router(const Router&)                = delete;
    Router& operator=(const Router&)     = delete;
    Router(Router&&) noexcept            = default;
    Router& operator=(Router&&) noexcept = default;

    void insert(HttpMethod, std::string path,
                const RouteHandler& routeHandler) const;

    [[nodiscard]] std::optional<RouteEntry> match(HttpMethod,
                                                  std::string path) const;

  private:
    mutable std::shared_mutex                mMutex {};
    std::map<HttpMethod, std::unique_ptr<TrieNode>> mMethodsMap;
};

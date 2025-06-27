#pragma once

#include <Skirnir/ServiceProvider.hpp>
#include <functional>
#include <map>
#include <regex>
#include <string>
#include <unordered_map>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using RouteHandler =
    std::function<void(HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>)>;

struct RouteEntry
{
    std::regex               extractParamsRegex;
    std::vector<std::string> paramsNames = {};
    RouteHandler             handler;

    std::unordered_map<std::string, std::string> extractRouteParams(
        const std::string& path) const
    {
        std::smatch                                  match;
        std::unordered_map<std::string, std::string> params;

        if (std::regex_match(path, match, extractParamsRegex))
        {
            for (size_t i = 0; i < paramsNames.size(); ++i)
            {
                params[paramsNames[i]] = match[i + 1];
            }
        }

        return std::move(params);
    }
};

struct TrieNode
{
    std::unordered_map<std::string, TrieNode*> children;
    std::optional<RouteEntry>                  routeEntry;
    bool                                       isEndOfPath = false;
};

class Router
{
  public:
    Router()
    {
        mMethodsMap = {
            { HttpMethod::GET, new TrieNode() },
            { HttpMethod::POST, new TrieNode() },
            { HttpMethod::PUT, new TrieNode() },
            { HttpMethod::DELETE, new TrieNode() },
            { HttpMethod::PATCH, new TrieNode() },
            { HttpMethod::OPTIONS, new TrieNode() },
            { HttpMethod::HEAD, new TrieNode() },
            { HttpMethod::TRACE, new TrieNode() },
            { HttpMethod::CONNECT, new TrieNode() },
        };
    }

    void insert(HttpMethod, std::string path,
                const RouteHandler& routeHandler) const;

    [[nodiscard]] std::optional<RouteEntry> match(HttpMethod,
                                                  std::string path) const;

  private:
    std::map<HttpMethod, TrieNode*> mMethodsMap;
};

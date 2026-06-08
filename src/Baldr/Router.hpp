#pragma once

#include <functional>
#include <map>
#include <regex>
#include <string>
#include <unordered_map>

#include <Skirnir/DependencyInjection/ServiceProvider.hpp>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using RouteHandler = std::function<skr::Task<>(
    HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>)>;

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
            { HttpMethod::Get, new TrieNode() },
            { HttpMethod::Post, new TrieNode() },
            { HttpMethod::Put, new TrieNode() },
            { HttpMethod::Delete, new TrieNode() },
            { HttpMethod::Patch, new TrieNode() },
            { HttpMethod::Options, new TrieNode() },
            { HttpMethod::Head, new TrieNode() },
            { HttpMethod::Trace, new TrieNode() },
            { HttpMethod::Connect, new TrieNode() },
        };
    }

    void insert(HttpMethod, std::string path,
                const RouteHandler& routeHandler) const;

    [[nodiscard]] std::optional<RouteEntry> match(HttpMethod,
                                                  std::string path) const;

  private:
    std::map<HttpMethod, TrieNode*> mMethodsMap;
};

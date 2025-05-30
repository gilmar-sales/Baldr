#pragma once

#include <Skirnir/ServiceProvider.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using RouteHandler =
    std::function<void(HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>)>;

struct TrieNode
{
    std::unordered_map<std::string, TrieNode*> children;
    std::optional<RouteHandler>                routeHandler;
    bool                                       isEndOfPath = false;
};

class PathMatcher
{
  public:
    PathMatcher()
    {
        mMethodsMap = {
            { "GET", new TrieNode() },     { "POST", new TrieNode() },
            { "PUT", new TrieNode() },     { "DELETE", new TrieNode() },
            { "PATCH", new TrieNode() },   { "OPTIONS", new TrieNode() },
            { "HEAD", new TrieNode() },    { "TRACE", new TrieNode() },
            { "CONNECT", new TrieNode() },
        };
    }

    void insert(const std::string& method, std::string path,
                const RouteHandler& routeHandler) const;

    [[nodiscard]] std::optional<RouteHandler> match(const std::string& method,
                                                    std::string path) const;

  private:
    std::unordered_map<std::string, TrieNode*> mMethodsMap;
};

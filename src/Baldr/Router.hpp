#pragma once

#include <Skirnir/ServiceProvider.hpp>
#include <functional>
#include <map>
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

    [[nodiscard]] std::optional<RouteHandler> match(HttpMethod,
                                                    std::string path) const;

  private:
    std::map<HttpMethod, TrieNode*> mMethodsMap;
};

#pragma once

#include <functional>
#include <memory>
#include <ServiceProvider.hpp>
#include <unordered_map>
#include <string>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using RouteHandler = std::function<void(HttpRequest&, HttpResponse&, std::shared_ptr<ServiceProvider>)>;

struct TrieNode {
    std::unordered_map<std::string, TrieNode *> children;
    std::optional<RouteHandler> routeHandler;
    bool isEndOfPath = false;
};

class PathMatcher {
public:
    PathMatcher() {
        mRoot = new TrieNode();
    }

    void insert(std::string path, const RouteHandler &routeHandler) const;

    [[nodiscard]] std::optional<RouteHandler> match(std::string path) const ;

private:
    TrieNode *mRoot;
};

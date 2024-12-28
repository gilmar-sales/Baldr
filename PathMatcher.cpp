#include "PathMatcher.hpp"

#include <stack>
#include <ranges>

void PathMatcher::insert(std::string path, const RouteHandler &routeHandler) const {
    TrieNode *current = mRoot;

    auto pathSegments = path
                        | std::views::split('/')
                        | std::views::filter([](const auto &s) {
                            return s.size() > 0;
                        });

    if (pathSegments.empty()) {
        for (const auto segment: {"/"}) {
            if (!current->children.contains(segment)) {
                current->children[segment] = new TrieNode();
            }
            current = current->children[segment];
        }
        current->routeHandler = routeHandler;
        current->isEndOfPath = true;
        return;
    }

    for (auto segment: pathSegments) {
        const auto &sv = std::string(segment.begin(), segment.end());

        if (!current->children.contains(sv)) {
            current->children[sv] = new TrieNode();
        }
        current = current->children[sv];
    }

    current->routeHandler = routeHandler;
    current->isEndOfPath = true;
}

std::optional<RouteHandler> PathMatcher::match(std::string path) const {

    auto  pathSegments  = path
                        | std::views::split('/')
                        | std::views::filter([](const auto &s) {
                            return s.size() > 0;
                        });

    if (pathSegments.empty()) {
        if (mRoot->children.contains("/") && mRoot->children["/"]->isEndOfPath) {
            return mRoot->children["/"]->routeHandler;
        }

        return {};
    }

    auto stack = std::stack<std::pair<TrieNode *, decltype(pathSegments.begin())> >({{mRoot, pathSegments.begin()}});

    while (!stack.empty()) {
        auto [node, index] = stack.top();
        stack.pop();

        if (index == pathSegments.end()) {
            if (node->isEndOfPath) {
                return node->routeHandler;
            }

            continue;
        }

        const auto &segment = std::string((*index).begin(), (*index).end());

        if (node->children.contains(segment)) {
            stack.emplace(node->children[segment], ++index);
        }

        if (node->children.contains("*")) {
            stack.emplace(node->children["*"], ++index);
        }
    }

    return {};
}

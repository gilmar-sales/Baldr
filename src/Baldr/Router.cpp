#include "Baldr/Router.hpp"

#include <ranges>
#include <stack>

void Router::insert(HttpMethod method, std::string path,
                    const RouteHandler& routeHandler) const
{
    std::unique_lock lock(mMutex);
    TrieNode*  current = mMethodsMap.at(method).get();
    RouteEntry routeEntry {
        .paramsNames = {},
        .hasParams   = !path.empty() && path.find(':') != std::string::npos,
        .handler     = routeHandler
    };

    auto pathSegments =
        path | std::views::split('/') |
        std::views::filter([](const auto& s) { return s.size() > 0; });

    if (pathSegments.empty())
    {
        for (const auto segment : { "/" })
        {
            if (!current->children.contains(segment))
            {
                current->children[segment] = std::make_unique<TrieNode>();
            }
            current = current->children[segment].get();
        }
        current->routeEntry  = routeEntry;
        current->isEndOfPath = true;
        return;
    }

    std::string regexStr = "^/";

    for (auto segment : pathSegments)
    {
        auto sv = std::string(segment.begin(), segment.end());

        if (sv.starts_with(':'))
        {
            routeEntry.paramsNames.emplace_back(sv.substr(1));
            regexStr += "([^/]+)/?";
            sv = "*";
        }
        else
        {
            regexStr += sv;
            regexStr += "/?";
        }

        if (!current->children.contains(sv))
        {
            current->children[sv] = std::make_unique<TrieNode>();
        }
        current = current->children[sv].get();
    }

    routeEntry.extractParamsRegex = std::regex(regexStr + "$");

    current->routeEntry  = routeEntry;
    current->isEndOfPath = true;
}

std::optional<RouteEntry> Router::match(HttpMethod  method,
                                        std::string path) const
{
    std::shared_lock lock(mMutex);
    auto pathSegments =
        path | std::views::split('/') |
        std::views::filter([](const auto& s) { return s.size() > 0; });

    auto root = mMethodsMap.at(method).get();

    if (pathSegments.empty())
    {
        if (root->children.contains("/") && root->children["/"]->isEndOfPath)
        {
            return root->children["/"]->routeEntry;
        }

        return {};
    }

    auto stack =
        std::stack<std::pair<TrieNode*, decltype(pathSegments.begin())>>(
            { { root, pathSegments.begin() } });

    while (!stack.empty())
    {
        auto [node, index] = stack.top();
        stack.pop();

        if (index == pathSegments.end())
        {
            if (node->isEndOfPath)
            {
                return node->routeEntry;
            }

            continue;
        }

        const auto& segment = std::string((*index).begin(), (*index).end());

        if (node->children.contains(segment))
        {
            stack.emplace(node->children[segment].get(), std::next(index));
        }

        if (node->children.contains("*"))
        {
            stack.emplace(node->children["*"].get(), std::next(index));
        }
    }

    return {};
}

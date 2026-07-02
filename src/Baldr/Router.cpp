#include "Baldr/Router.hpp"

#include <ranges>
#include <stack>
#include <stdexcept>

void Router::insert(HttpMethod method, std::string path,
                    const RouteHandler& routeHandler) const
{
    std::unique_lock lock(mMutex);
    TrieNode*  current = mMethodsMap.at(method).get();
    RouteEntry routeEntry {
        .paramsNames = {},
        .hasParams   = !path.empty() &&
                     (path.find(':') != std::string::npos ||
                      path.find("**") != std::string::npos),
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

    std::string regexStr  = "^/";
    bool        greedySet = false;

    for (auto segment : pathSegments)
    {
        auto sv = std::string(segment.begin(), segment.end());

        if (sv == "**")
        {
            if (greedySet)
                throw std::invalid_argument(
                    "Router: '**' may only appear once per route");

            greedySet = true;
            routeEntry.paramsNames.emplace_back("filepath");
            regexStr += "(?:/(.*))?";
            sv = "**";
        }
        else if (sv.starts_with(':'))
        {
            if (greedySet)
                throw std::invalid_argument(
                    "Router: '**' must be the final segment");

            routeEntry.paramsNames.emplace_back(sv.substr(1));
            regexStr += "([^/]+)/?";
            sv = "*";
        }
        else
        {
            if (greedySet)
                throw std::invalid_argument(
                    "Router: '**' must be the final segment");

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

std::optional<RouteEntry> Router::matchInTrie(HttpMethod         method,
                                                std::string_view path) const
{
    auto root = mMethodsMap.at(method).get();

    auto pathSegments =
        path | std::views::split('/') |
        std::views::filter([](const auto& s) { return s.size() > 0; });

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

            if (node->children.contains("**") &&
                node->children["**"]->isEndOfPath)
            {
                return node->children["**"]->routeEntry;
            }

            continue;
        }

        const auto& segment = std::string((*index).begin(), (*index).end());

        if (node->children.contains("**") && node->children["**"]->isEndOfPath)
        {
            stack.emplace(node->children["**"].get(), pathSegments.end());
        }

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

std::optional<RouteEntry> Router::match(HttpMethod  method,
                                        std::string path) const
{
    std::shared_lock lock(mMutex);
    return matchInTrie(method, path);
}

Router::MatchResult Router::matchWithAllow(HttpMethod  method,
                                           std::string path) const
{
    std::shared_lock lock(mMutex);

    MatchResult result;
    result.entry = matchInTrie(method, path);

    // HEAD falls back to GET when no HEAD route is registered.
    if (!result.entry.has_value() && method == HttpMethod::Head)
    {
        auto getEntry = matchInTrie(HttpMethod::Get, path);
        if (getEntry.has_value())
        {
            result.entry          = getEntry;
            result.resolvedMethod = HttpMethod::Get;
        }
    }
    else
    {
        result.resolvedMethod = method;
    }

    if (result.entry.has_value())
        return result;

    for (const auto& [m, _] : mMethodsMap)
    {
        if (m == method)
            continue;
        if (matchInTrie(m, path).has_value())
            result.allowedMethodsOnPath.push_back(m);
    }

    return result;
}

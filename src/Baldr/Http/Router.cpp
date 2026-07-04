#include <Baldr/Http/Router.hpp>

#include <ranges>
#include <stack>
#include <stdexcept>
#include <utility>

#include <Baldr/OpenApi/JsonSchemaEmitter.hpp>

namespace
{
    RouteEntry makeEntry(HttpMethod method, std::string_view path,
                         Baldr::RouteOptions options, std::string groupPrefix,
                         const RouteHandler& handler)
    {
        return RouteEntry {
            .paramsNames = {},
            .hasParams   = !path.empty() &&
                         (path.find(':') != std::string::npos ||
                           path.find("**") != std::string::npos),
            .handler      = handler,
            .options      = std::move(options),
            .groupPrefix  = std::move(groupPrefix),
            .pathTemplate = std::string(path),
            .method       = method,
        };
    }
}

Router::Router() : mSchemaRegistry(
                       skr::MakeArc<SchemaRegistry>())
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

void Router::insert(HttpMethod method, std::string path,
                    const RouteHandler& routeHandler) const
{
    insert(method, std::move(path), Baldr::RouteOptions {}, "",
           routeHandler);
}

void Router::insert(HttpMethod method, std::string path,
                    Baldr::RouteOptions options,
                    std::string groupPrefix,
                    const RouteHandler& routeHandler) const
{
    std::unique_lock lock(mMutex);
    TrieNode*  current = mMethodsMap.at(method).get();
    RouteEntry routeEntry = makeEntry(method, path, std::move(options),
                                      std::move(groupPrefix), routeHandler);

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
    std::string unused;
    return matchInTrieWithTemplate(method, path, unused);
}

std::optional<RouteEntry> Router::matchInTrieWithTemplate(
    HttpMethod method, std::string_view path, std::string& outTemplate) const
{
    auto root = mMethodsMap.at(method).get();

    auto pathSegments =
        path | std::views::split('/') |
        std::views::filter([](const auto& s) { return s.size() > 0; });

    auto joinTemplate = [](const std::vector<std::string>& parts) {
        std::string out;
        for (const auto& p : parts)
        {
            out.push_back('/');
            out.append(p);
        }
        if (out.empty())
            return std::string("/");
        return out;
    };

    if (pathSegments.empty())
    {
        if (root->children.contains("/") && root->children["/"]->isEndOfPath)
        {
            outTemplate = "/";
            return root->children["/"]->routeEntry;
        }

        return {};
    }

    struct Frame
    {
        TrieNode*                   node;
        decltype(pathSegments.begin()) index;
        std::vector<std::string>    parts;
    };

    auto stack = std::stack<Frame>(
        { { root, pathSegments.begin(), {} } });

    while (!stack.empty())
    {
        auto [node, index, parts] = stack.top();
        stack.pop();

        if (index == pathSegments.end())
        {
            if (node->isEndOfPath)
            {
                outTemplate = joinTemplate(parts);
                return node->routeEntry;
            }

            if (node->children.contains("**") &&
                node->children["**"]->isEndOfPath)
            {
                auto childParts = parts;
                childParts.emplace_back("**");
                outTemplate = joinTemplate(childParts);
                return node->children["**"]->routeEntry;
            }

            continue;
        }

        const auto& segment = std::string((*index).begin(), (*index).end());

        if (node->children.contains("**") && node->children["**"]->isEndOfPath)
        {
            auto childParts = parts;
            childParts.emplace_back("**");
            stack.push(Frame { node->children["**"].get(), pathSegments.end(),
                              std::move(childParts) });
        }

        if (node->children.contains(segment))
        {
            auto childParts = parts;
            childParts.emplace_back(segment);
            stack.push(Frame { node->children[segment].get(),
                              std::next(index), std::move(childParts) });
        }

        if (node->children.contains("*"))
        {
            auto childParts = parts;
            childParts.emplace_back("*");
            stack.push(Frame { node->children["*"].get(), std::next(index),
                              std::move(childParts) });
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
    if (result.entry.has_value())
        result.routeTemplate = result.entry->pathTemplate;

    // HEAD falls back to GET when no HEAD route is registered.
    if (!result.entry.has_value() && method == HttpMethod::Head)
    {
        auto getEntry = matchInTrie(HttpMethod::Get, path);
        if (getEntry.has_value())
        {
            result.entry          = getEntry;
            result.resolvedMethod = HttpMethod::Get;
            result.routeTemplate  = getEntry->pathTemplate;
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

std::vector<RouteEntry> Router::Snapshot() const
{
    std::shared_lock lock(mMutex);
    std::vector<RouteEntry> out;

    for (const auto& [method, root] : mMethodsMap)
    {
        struct Frame
        {
            TrieNode*                  node;
            std::vector<std::string>   parts;
        };
        std::stack<Frame> stack;
        stack.push(Frame { root.get(), {} });

        while (!stack.empty())
        {
            auto [node, parts] = stack.top();
            stack.pop();

            if (node->isEndOfPath && node->routeEntry.has_value())
            {
                std::string tmpl;
                if (parts.empty())
                    tmpl = "/";
                else
                {
                    for (const auto& p : parts)
                    {
                        tmpl.push_back('/');
                        tmpl.append(p);
                    }
                }

                auto entry = *node->routeEntry;
                if (entry.pathTemplate.empty())
                    entry.pathTemplate = tmpl;
                out.push_back(std::move(entry));
            }

            for (const auto& [key, child] : node->children)
            {
                auto childParts = parts;
                childParts.emplace_back(key);
                stack.push(Frame { child.get(), std::move(childParts) });
            }
        }
    }

    return out;
}

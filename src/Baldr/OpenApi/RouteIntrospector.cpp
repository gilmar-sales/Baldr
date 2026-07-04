#include "RouteIntrospector.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <unordered_set>

const char* MethodToString(HttpMethod m)
{
    switch (m)
    {
        case HttpMethod::Get:
            return "get";
        case HttpMethod::Post:
            return "post";
        case HttpMethod::Put:
            return "put";
        case HttpMethod::Delete:
            return "delete";
        case HttpMethod::Patch:
            return "patch";
        case HttpMethod::Head:
            return "head";
        case HttpMethod::Options:
            return "options";
        case HttpMethod::Trace:
            return "trace";
        case HttpMethod::Connect:
            return "connect";
    }
    return "get";
}

std::string TranslatePath(const std::string& routerPath)
{
    std::string out;
    out.reserve(routerPath.size() + 8);

    size_t i = 0;
    while (i < routerPath.size())
    {
        char c = routerPath[i];
        if (c == ':')
        {
            out.push_back('{');
            ++i;
            while (i < routerPath.size() && routerPath[i] != '/')
            {
                out.push_back(routerPath[i]);
                ++i;
            }
            out.push_back('}');
        }
        else if (c == '*' && i + 1 < routerPath.size() &&
                 routerPath[i + 1] == '*')
        {
            if (!out.empty() && out.back() != '/')
                out.push_back('/');
            out += "{filepath}";
            i += 2;
        }
        else
        {
            out.push_back(c);
            ++i;
        }
    }
    if (out.empty())
        return "/";
    return out;
}

std::vector<std::string> UniquePaths(const std::vector<RouteEntry>& entries)
{
    std::vector<std::string>        out;
    std::unordered_set<std::string> seen;
    for (const auto& e : entries)
    {
        std::string p = TranslatePath(e.pathTemplate);
        if (seen.insert(p).second)
            out.push_back(std::move(p));
    }
    return out;
}
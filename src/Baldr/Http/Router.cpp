#include <Baldr/Detail/Namespace.hpp>
#include <Baldr/Http/Router.hpp>

#include <map>
#include <mutex>
#include <shared_mutex>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <Baldr/OpenApi/JsonSchemaEmitter.hpp>

namespace BALDR_NAMESPACE
{

    namespace
    {
        std::vector<std::string_view> splitPath(std::string_view path)
        {
            std::vector<std::string_view> out;
            std::size_t                   pos = 0;
            // Skip the leading slash so the first segment isn't empty.
            if (!path.empty() && path.front() == '/')
                pos = 1;
            while (pos <= path.size())
            {
                std::size_t slash = path.find('/', pos);
                if (slash == std::string_view::npos)
                {
                    if (pos < path.size())
                        out.push_back(path.substr(pos));
                    break;
                }
                if (slash > pos)
                    out.push_back(path.substr(pos, slash - pos));
                pos = slash + 1;
            }
            return out;
        }
    } // namespace

    struct TrieNode
    {
        std::unordered_map<std::string, std::unique_ptr<TrieNode>> children;
        std::optional<RouteEntry>                                  routeEntry;
        bool isEndOfPath = false;

        TrieNode()                               = default;
        TrieNode(TrieNode&&) noexcept            = default;
        TrieNode& operator=(TrieNode&&) noexcept = default;
        TrieNode(const TrieNode&)                = delete;
        TrieNode& operator=(const TrieNode&)     = delete;

        ~TrieNode() = default;
    };

    std::unordered_map<std::string, std::string> RouteEntry::extractRouteParams(
        const std::string& path) const
    {
        std::unordered_map<std::string, std::string> params;
        if (!hasParams || paramSegments.empty())
            return params;

        const std::vector<std::string_view> segs = splitPath(path);
        const std::size_t                   n    = paramSegments.size();
        std::size_t                         si   = 0;

        for (std::size_t pi = 0; pi < n; ++pi)
        {
            const ParamSegment& seg = paramSegments[pi];
            if (seg.kind == SegmentKind::Greedy)
            {
                params[seg.text] = segs.empty() ? std::string {} : [&]() {
                    std::string joined;
                    for (std::size_t i = si; i < segs.size(); ++i)
                    {
                        if (i > si)
                            joined.push_back('/');
                        joined.append(segs[i]);
                    }
                    return joined;
                }();
                return params;
            }
            if (si >= segs.size())
                return params;
            if (seg.kind == SegmentKind::Literal)
            {
                if (segs[si] != seg.text)
                    return params;
            }
            else
            {
                params[seg.text] = std::string(segs[si]);
            }
            ++si;
        }
        // Trailing literal segments after a `:name` must consume exactly
        // the same number of segments — already enforced above.
        if (si != segs.size())
            return params;
        return params;
    }

    struct Router::Impl
    {
        mutable std::shared_mutex                       mMutex {};
        std::map<HttpMethod, std::unique_ptr<TrieNode>> mMethodsMap;
        skr::Arc<SchemaRegistry>                        mSchemaRegistry;
    };

    RouteEntry makeEntry(HttpMethod method, std::string_view path,
                         RouteOptions options, std::string groupPrefix,
                         const RouteHandler& handler)
    {
        return RouteEntry {
            .paramsNames = {},
            .hasParams =
                !path.empty() && (path.find(':') != std::string::npos ||
                                  path.find("**") != std::string::npos),
            .handler      = handler,
            .options      = std::move(options),
            .groupPrefix  = std::move(groupPrefix),
            .pathTemplate = std::string(path),
            .method       = method,
        };
    }

    constexpr std::size_t kMaxWildcardSegments = 32;

    std::optional<RouteEntry> matchInTrieWithTemplate(
        const std::map<HttpMethod, std::unique_ptr<TrieNode>>& mMethodsMap,
        HttpMethod                                             method,
        std::string_view                                       path,
        std::string&                                           outTemplate)
    {
        auto root = mMethodsMap.at(method).get();

        const std::vector<std::string_view> pathSegments = splitPath(path);

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
            if (root->children.contains("/") &&
                root->children["/"]->isEndOfPath)
            {
                outTemplate = "/";
                return root->children["/"]->routeEntry;
            }

            return {};
        }

        struct Frame
        {
            TrieNode*                node;
            std::size_t              index;
            std::vector<std::string> parts;
            std::size_t              partsSize;
        };

        std::vector<Frame> stack;
        stack.reserve(8);
        stack.push_back({ root, 0, {}, 0 });

        while (!stack.empty())
        {
            Frame frame = std::move(stack.back());
            stack.pop_back();

            TrieNode*        node      = frame.node;
            std::size_t      index     = frame.index;
            std::size_t      partsSize = frame.partsSize;
            std::vector<std::string> parts = std::move(frame.parts);
            parts.resize(partsSize);

            if (index == pathSegments.size())
            {
                if (node->isEndOfPath)
                {
                    outTemplate = joinTemplate(parts);
                    return node->routeEntry;
                }

                if (node->children.contains("**") &&
                    node->children.at("**")->isEndOfPath)
                {
                    parts.emplace_back("**");
                    outTemplate = joinTemplate(parts);
                    return node->children.at("**")->routeEntry;
                }

                continue;
            }

            const std::string segment(pathSegments[index]);

            auto pushFrame = [&](TrieNode* nextNode, std::size_t nextIndex,
                                 std::size_t nextPartsSize, std::string extra) {
                Frame next { nextNode, nextIndex, {}, nextPartsSize };
                next.parts.reserve(nextPartsSize);
                for (std::size_t i = 0; i < partsSize; ++i)
                    next.parts.push_back(std::move(parts[i]));
                if (!extra.empty())
                    next.parts.emplace_back(std::move(extra));
                stack.push_back(std::move(next));
            };

            if (node->children.contains("**") &&
                node->children.at("**")->isEndOfPath)
            {
                pushFrame(node->children.at("**").get(),
                          pathSegments.size(), partsSize + 1, "**");
            }

            if (node->children.contains(segment))
            {
                pushFrame(node->children.at(segment).get(), index + 1,
                          partsSize + 1, std::string(segment));
            }

            if (node->children.contains("*"))
            {
                pushFrame(node->children.at("*").get(), index + 1,
                          partsSize + 1, "*");
            }
        }

        return {};
    }

    Router::Router() : mImpl(std::make_unique<Impl>())
    {
        mImpl->mSchemaRegistry                 = skr::MakeArc<SchemaRegistry>();
        mImpl->mMethodsMap[HttpMethod::Get]    = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Post]   = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Put]    = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Delete] = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Patch]  = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Options] = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Head]    = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Trace]   = std::make_unique<TrieNode>();
        mImpl->mMethodsMap[HttpMethod::Connect] = std::make_unique<TrieNode>();
    }

    Router::~Router() = default;

    void Router::insert(HttpMethod method, std::string path,
                        const RouteHandler& routeHandler) const
    {
        insert(method, std::move(path), RouteOptions {}, "", routeHandler);
    }

    void Router::insert(HttpMethod method, std::string path,
                        RouteOptions options, std::string groupPrefix,
                        const RouteHandler& routeHandler) const
    {
        std::unique_lock lock(mImpl->mMutex);
        TrieNode*        current = mImpl->mMethodsMap.at(method).get();
        RouteEntry      routeEntry =
            makeEntry(method, path, std::move(options),
                      std::move(groupPrefix), routeHandler);

        const std::vector<std::string_view> pathSegments = splitPath(path);

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

        bool        greedySet     = false;
        std::size_t wildcardCount = 0;

        for (std::string_view sv : pathSegments)
        {
            std::string trieKey;
            if (sv == "**")
            {
                if (greedySet)
                    throw std::invalid_argument(
                        "Router: '**' may only appear once per route");

                greedySet = true;
                ++wildcardCount;
                if (wildcardCount > kMaxWildcardSegments)
                    throw std::invalid_argument(
                        "Router: route contains too many wildcard segments");
                routeEntry.paramsNames.emplace_back("filepath");
                routeEntry.paramSegments.push_back(
                    { RouteEntry::SegmentKind::Greedy, "filepath" });
                trieKey = "**";
            }
            else if (sv.size() > 1 && sv.front() == ':')
            {
                if (greedySet)
                    throw std::invalid_argument(
                        "Router: '**' must be the final segment");

                ++wildcardCount;
                if (wildcardCount > kMaxWildcardSegments)
                    throw std::invalid_argument(
                        "Router: route contains too many wildcard segments");
                const std::string name(sv.substr(1));
                routeEntry.paramsNames.push_back(name);
                routeEntry.paramSegments.push_back(
                    { RouteEntry::SegmentKind::Single, name });
                trieKey = "*";
            }
            else
            {
                if (greedySet)
                    throw std::invalid_argument(
                        "Router: '**' must be the final segment");
                routeEntry.paramSegments.push_back(
                    { RouteEntry::SegmentKind::Literal, std::string(sv) });
                trieKey = std::string(sv);
            }

            if (!current->children.contains(trieKey))
            {
                current->children[trieKey] = std::make_unique<TrieNode>();
            }
            current = current->children[trieKey].get();
        }

        current->routeEntry  = std::move(routeEntry);
        current->isEndOfPath = true;
    }

    std::optional<RouteEntry> Router::match(HttpMethod  method,
                                            std::string path) const
    {
        std::shared_lock lock(mImpl->mMutex);
        std::string      unused;
        return matchInTrieWithTemplate(
            mImpl->mMethodsMap, method, path, unused);
    }

    Router::MatchResult Router::matchWithAllow(HttpMethod  method,
                                               std::string path) const
    {
        std::shared_lock lock(mImpl->mMutex);

        MatchResult result;
        std::string template_;
        result.entry = matchInTrieWithTemplate(
            mImpl->mMethodsMap, method, path, template_);
        if (result.entry.has_value())
            result.routeTemplate = result.entry->pathTemplate;

        if (!result.entry.has_value() && method == HttpMethod::Head)
        {
            auto getEntry = matchInTrieWithTemplate(
                mImpl->mMethodsMap, HttpMethod::Get, path, template_);
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

        for (const auto& [m, _] : mImpl->mMethodsMap)
        {
            if (m == method)
                continue;
            if (matchInTrieWithTemplate(mImpl->mMethodsMap, m, path, template_)
                    .has_value())
                result.allowedMethodsOnPath.push_back(m);
        }

        return result;
    }

    std::vector<RouteEntry> Router::Snapshot() const
    {
        std::shared_lock        lock(mImpl->mMutex);
        std::vector<RouteEntry> out;

        for (const auto& [method, root] : mImpl->mMethodsMap)
        {
            struct Frame
            {
                TrieNode*                node;
                std::vector<std::string> parts;
                std::size_t              partsSize;
            };
            std::vector<Frame> stack;
            stack.push_back(Frame { root.get(), {}, 0 });

            while (!stack.empty())
            {
                Frame frame = std::move(stack.back());
                stack.pop_back();

                TrieNode*                node      = frame.node;
                std::size_t              partsSize = frame.partsSize;
                std::vector<std::string> parts     = std::move(frame.parts);
                parts.resize(partsSize);

                if (node->isEndOfPath && node->routeEntry.has_value())
                {
                    std::string tmpl;
                    if (partsSize == 0)
                        tmpl = "/";
                    else
                    {
                        for (std::size_t i = 0; i < partsSize; ++i)
                        {
                            tmpl.push_back('/');
                            tmpl.append(parts[i]);
                        }
                    }

                    auto entry = *node->routeEntry;
                    if (entry.pathTemplate.empty())
                        entry.pathTemplate = tmpl;
                    out.push_back(std::move(entry));
                }

                for (const auto& [key, child] : node->children)
                {
                    Frame next { child.get(), {}, partsSize + 1 };
                    next.parts.reserve(next.partsSize);
                    for (std::size_t i = 0; i < partsSize; ++i)
                        next.parts.push_back(std::move(parts[i]));
                    next.parts.emplace_back(key);
                    stack.push_back(std::move(next));
                }
            }
        }

        return out;
    }

    const skr::Arc<SchemaRegistry>& Router::SchemaRegistrySlot() const
    {
        return mImpl->mSchemaRegistry;
    }

} // namespace BALDR_NAMESPACE
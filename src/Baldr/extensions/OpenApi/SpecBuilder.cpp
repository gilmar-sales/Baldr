#include "SpecBuilder.hpp"

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

#include "RouteIntrospector.hpp"

namespace Baldr::OpenApi
{
    namespace
    {
        std::string escapeString(const std::string& s)
        {
            std::string out;
            out.reserve(s.size() + 2);
            for (char c : s)
            {
                switch (c)
                {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20)
                        {
                            char buf[8];
                            std::snprintf(buf, sizeof(buf), "\\u%04x",
                                          static_cast<unsigned char>(c));
                            out += buf;
                        }
                        else
                        {
                            out.push_back(c);
                        }
                }
            }
            return out;
        }

        std::string rawOrRef(const std::string& schemaJson)
        {
            return schemaJson;
        }
    }

    std::string SpecBuilder::Render(const std::vector<RouteEntry>& entries)
    {
        std::string out;
        out.reserve(4096);

        out += "{\"openapi\":\"3.0.3\",";
        out += "\"info\":{";
        out += "\"title\":\"" + escapeString(mOptions.info.title) + "\",";
        out += "\"version\":\"" + escapeString(mOptions.info.version) + "\"";
        if (mOptions.info.description.has_value())
        {
            out += ",\"description\":\"";
            out += escapeString(*mOptions.info.description);
            out += "\"";
        }
        out += "},";

        // Tags (alphabetical, deduped).
        std::set<std::string> tagSet;
        for (const auto& e : entries)
            for (const auto& t : e.options.tags)
                tagSet.insert(t);
        out += "\"tags\":[";
        bool first = true;
        for (const auto& t : tagSet)
        {
            if (!first)
                out += ",";
            first = false;
            out += "{\"name\":\"" + escapeString(t) + "\"}";
        }
        out += "],";

        // Group routes by translated path template; each template holds
        // a list of (method, entry) pairs.
        std::map<std::string, std::vector<std::pair<std::string,
                                                    const RouteEntry*>>>
            byPath;
        for (const auto& e : entries)
        {
            std::string tp = TranslatePath(e.pathTemplate);
            byPath[tp].emplace_back(MethodToString(e.method), &e);
        }

        out += "\"paths\":{";
        first = true;
        for (const auto& [path, ops] : byPath)
        {
            if (!first)
                out += ",";
            first = false;
            out += "\"" + escapeString(path) + "\":{";
            bool firstOp = true;
            for (const auto& [verb, entry] : ops)
            {
                if (!firstOp)
                    out += ",";
                firstOp = false;

                out += "\"" + verb + "\":{";
                if (entry->options.summary.has_value())
                {
                    out += "\"summary\":\"";
                    out += escapeString(*entry->options.summary);
                    out += "\",";
                }
                if (entry->options.description.has_value())
                {
                    out += "\"description\":\"";
                    out += escapeString(*entry->options.description);
                    out += "\",";
                }
                if (entry->options.operationId.has_value())
                {
                    out += "\"operationId\":\"";
                    out += escapeString(*entry->options.operationId);
                    out += "\",";
                }
                if (entry->options.deprecated)
                    out += "\"deprecated\":true,";

                if (!entry->options.tags.empty())
                {
                    out += "\"tags\":[";
                    for (size_t i = 0; i < entry->options.tags.size(); ++i)
                    {
                        if (i > 0)
                            out += ",";
                        out += "\"";
                        out += escapeString(entry->options.tags[i]);
                        out += "\"";
                    }
                    out += "],";
                }

                bool hasBody = false;
                std::string bodyContent;
                if (auto it = entry->options.metadata.find(
                        "responseSchemaJson");
                    it != entry->options.metadata.end() &&
                    !it->second.empty())
                {
                    hasBody = true;
                    bodyContent = "{\"application/json\":{\"schema\":";
                    bodyContent += rawOrRef(it->second);
                    bodyContent += "}}";
                }

                if (hasBody)
                {
                    out += "\"responses\":{\"200\":{\"description\":\"OK\","
                           "\"content\":";
                    out += bodyContent;
                    out += "}}";
                }
                else
                {
                    out += "\"responses\":{\"200\":{\"description\":\"OK\"}}";
                }

                out += "}";
            }
            out += "}";
        }
        out += "},";

        // Components.
        out += "\"components\":{";
        if (mRegistry && !mRegistry->Schemas().empty())
        {
            out += "\"schemas\":";
            out += mRegistry->RenderComponents();
        }
        out += "}}";

        return out;
    }
}
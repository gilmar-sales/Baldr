#include "SpecBuilder.hpp"
#include <Baldr/Detail/Namespace.hpp>

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <simdjson.h>

#include "RouteIntrospector.hpp"

namespace BALDR_NAMESPACE
{

    std::string escapeString(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 2);
        for (char c : s)
        {
            switch (c)
            {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
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
        std::map<std::string,
                 std::vector<std::pair<std::string, const RouteEntry*>>>
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

                bool        hasBody = false;
                std::string bodySchema;
                if (auto it =
                        entry->options.metadata.find("responseSchemaJson");
                    it != entry->options.metadata.end() && !it->second.empty())
                {
                    hasBody    = true;
                    bodySchema = rawOrRef(it->second);
                }

                std::map<std::string, std::string> perStatus;
                std::map<std::string, std::string> contentTypeByStatus;

                auto consumeStatusMap = [&perStatus](const std::string& blob) {
                    simdjson::dom::parser  parser;
                    simdjson::dom::element mapEl;
                    if (parser.parse(blob).get(mapEl))
                        return;
                    simdjson::dom::object mapObj;
                    if (mapEl.get_object().get(mapObj))
                        return;
                    for (auto kv : mapObj)
                    {
                        std::string_view      statusSv = kv.key;
                        simdjson::dom::object entryObj;
                        if (kv.value.get_object().get(entryObj))
                            continue;
                        auto schemaEl = entryObj["schema"];
                        if (!schemaEl.is_object())
                            continue;
                        simdjson::dom::object schemaObj;
                        if (schemaEl.get_object().get(schemaObj))
                            continue;
                        std::string raw = simdjson::minify(schemaObj);
                        if (raw == "{}")
                        {
                            perStatus[std::string(statusSv)] = std::string();
                        }
                        else
                        {
                            perStatus[std::string(statusSv)] = std::move(raw);
                        }
                    }
                };

                if (auto sit = entry->options.metadata.find(
                        "responseStatusSchemasJson");
                    sit != entry->options.metadata.end() &&
                    !sit->second.empty())
                {
                    consumeStatusMap(sit->second);
                }

                if (auto sit =
                        entry->options.metadata.find("responseSchemasJson");
                    sit != entry->options.metadata.end() &&
                    !sit->second.empty())
                {
                    consumeStatusMap(sit->second);
                }

                if (auto cit = entry->options.metadata.find(
                        "responseContentTypesJson");
                    cit != entry->options.metadata.end() &&
                    !cit->second.empty())
                {
                    simdjson::dom::parser  parser;
                    simdjson::dom::element mapEl;
                    if (!parser.parse(cit->second).get(mapEl))
                    {
                        simdjson::dom::object mapObj;
                        if (!mapEl.get_object().get(mapObj))
                        {
                            for (auto kv : mapObj)
                            {
                                std::string_view statusSv = kv.key;
                                std::string_view mimeSv;
                                if (kv.value.get_string().get(mimeSv))
                                    continue;
                                contentTypeByStatus[std::string(statusSv)] =
                                    std::string(mimeSv);
                            }
                        }
                    }
                }

                std::string singleContentType = "application/json";
                if (auto mit =
                        entry->options.metadata.find("responseContentType");
                    mit != entry->options.metadata.end() &&
                    !mit->second.empty())
                {
                    singleContentType = mit->second;
                }

                if (hasBody)
                {
                    perStatus.try_emplace("200", bodySchema);
                    if (contentTypeByStatus.find("200") ==
                        contentTypeByStatus.end())
                    {
                        contentTypeByStatus["200"] = singleContentType;
                    }
                }

                std::string parametersBlock;
                if (auto qit =
                        entry->options.metadata.find("queryParametersJson");
                    qit != entry->options.metadata.end() &&
                    !qit->second.empty())
                {
                    parametersBlock += qit->second;
                }
                if (auto pit =
                        entry->options.metadata.find("pathParametersJson");
                    pit != entry->options.metadata.end() &&
                    !pit->second.empty())
                {
                    if (!parametersBlock.empty())
                        parametersBlock += ",";
                    parametersBlock += pit->second;
                }

                if (!parametersBlock.empty())
                {
                    out += "\"parameters\":[";
                    out += parametersBlock;
                    out += "],";
                }

                auto renderResponses =
                    [&](const std::map<std::string, std::string>& statuses,
                        const std::map<std::string, std::string>& ctypes) {
                        out += "\"responses\":{";
                        bool firstStatus = true;
                        for (const auto& [statusStr, schemaStr] : statuses)
                        {
                            if (!firstStatus)
                                out += ",";
                            firstStatus = false;
                            out += "\"";
                            out += escapeString(statusStr);
                            out += "\":{\"description\":\"";
                            out += escapeString(statusStr);
                            out += "\",";

                            std::string mime;
                            if (auto cit2 = ctypes.find(statusStr);
                                cit2 != ctypes.end())
                            {
                                mime = cit2->second;
                            }
                            else
                            {
                                mime = singleContentType;
                            }

                            if (schemaStr.empty())
                            {
                                out += "\"content\":{}}";
                            }
                            else if (mime.empty())
                            {
                                out += "\"content\":{}}";
                            }
                            else
                            {
                                out += "\"content\":{\"";
                                out += escapeString(mime);
                                out += "\":{\"schema\":";
                                out += schemaStr;
                                out += "}}}";
                            }
                        }
                        out += "}";
                    };

                if (!perStatus.empty())
                {
                    renderResponses(perStatus, contentTypeByStatus);
                }
                else if (hasBody)
                {
                    std::map<std::string, std::string> singleton;
                    singleton["200"] = bodySchema;
                    std::map<std::string, std::string> singletonMime;
                    singletonMime["200"] = singleContentType;
                    renderResponses(singleton, singletonMime);
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

} // namespace BALDR_NAMESPACE
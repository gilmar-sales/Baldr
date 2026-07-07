#ifndef NDEBUG

    #include <Baldr/Application/RouteListing.hpp>
    #include <Baldr/Detail/Namespace.hpp>
    #include <Baldr/Http/Method.hpp>

    #include <Skirnir/Common/Reflection.hpp>
    #include <string>

namespace BALDR_NAMESPACE
{

    namespace
    {
        std::string jsonEscapeRoute(std::string_view s)
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
                    case '\b':
                        out += "\\b";
                        break;
                    case '\f':
                        out += "\\f";
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

        std::string metadataToJson(
            const std::unordered_map<std::string, std::string>& m)
        {
            std::string out;
            out += '{';
            bool first = true;
            for (const auto& [k, v] : m)
            {
                if (!first)
                    out += ',';
                first = false;
                out += '"';
                out += jsonEscapeRoute(k);
                out += "\":\"";
                out += jsonEscapeRoute(v);
                out += '"';
            }
            out += '}';
            return out;
        }
    } // namespace

    std::string RouteListingToJson(const std::vector<RouteEntry>& entries)
    {
        std::string out;
        out.reserve(64 + entries.size() * 96);
        out += "{\"routes\":[";
        bool first = true;
        for (const auto& e : entries)
        {
            if (!first)
                out += ',';
            first = false;
            out += "{\"method\":\"";
            out += refl::enum_to_string(e.method);
            out += "\",\"path\":\"";
            out += jsonEscapeRoute(e.pathTemplate);
            out += "\",\"group\":\"";
            out += jsonEscapeRoute(e.groupPrefix);
            out += "\",\"metadata\":";
            out += metadataToJson(e.options.metadata);
            out += '}';
        }
        out += "]}";
        return out;
    }

} // namespace BALDR_NAMESPACE

#endif // !NDEBUG
#include "Method.hpp"

namespace BALDR_NAMESPACE
{

    std::optional<HttpMethod> parseMethod(std::string_view method)
    {
        if (method == "GET")
            return HttpMethod::Get;
        if (method == "POST")
            return HttpMethod::Post;
        if (method == "PUT")
            return HttpMethod::Put;
        if (method == "DELETE")
            return HttpMethod::Delete;
        if (method == "PATCH")
            return HttpMethod::Patch;
        if (method == "OPTIONS")
            return HttpMethod::Options;
        if (method == "HEAD")
            return HttpMethod::Head;
        if (method == "TRACE")
            return HttpMethod::Trace;
        if (method == "CONNECT")
            return HttpMethod::Connect;

        return std::nullopt;
    }

    const char* MethodToString(HttpMethod m)
    {
        switch (m)
        {
            case HttpMethod::Get:
                return "GET";
            case HttpMethod::Post:
                return "POST";
            case HttpMethod::Put:
                return "PUT";
            case HttpMethod::Delete:
                return "DELETE";
            case HttpMethod::Patch:
                return "PATCH";
            case HttpMethod::Head:
                return "HEAD";
            case HttpMethod::Options:
                return "OPTIONS";
            case HttpMethod::Trace:
                return "TRACE";
            case HttpMethod::Connect:
                return "CONNECT";
        }

        return "";
    }

} // namespace BALDR_NAMESPACE
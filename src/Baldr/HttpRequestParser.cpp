#include "Baldr/StringHelpers.hpp"
#include "rfl/enums.hpp"
#include <ranges>
#include <sstream>

#include "Baldr/HttpRequestParser.hpp"
#include "Baldr/StatusCode.hpp"

HttpResult<HttpRequest> HttpRequestParser::parse(const std::string& request)
{
    auto result =
        HttpResult<HttpRequest> { .success    = false,
                                  .error      = "Empty request",
                                  .statusCode = StatusCode::BadRequest };

    std::istringstream requestStream(request);

    std::string httpMethod;

    requestStream >> httpMethod;

    auto parsedHttpMethod = rfl::string_to_enum<HttpMethod>(httpMethod);

    if (!parsedHttpMethod.has_value())
    {
        result.error = "Malformed HTTP method";
        return result;
    }

    result.value.method = parsedHttpMethod.value();

    requestStream.get();
    if (request.at(requestStream.tellg()) == ' ')
    {
        result.error = "Extra whitespace in method";
        return result;
    }

    requestStream >> result.value.path;

    requestStream.get();
    if (request.at(requestStream.tellg()) == ' ')
    {
        result.error = "Extra whitespace in path";
        return result;
    }

    requestStream >> result.value.version;

    if (!result.value.version.starts_with("HTTP/"))
    {
        result.error = "HTTP version is missing or invalid";
        return result;
    }

    if (request.at(requestStream.tellg()) == ' ')
    {
        result.error = "Extra whitespace in version";
        return result;
    }

    auto queryIndex = result.value.path.find('?');

    if (queryIndex != std::string::npos)
    {
        auto params =
            std::string_view(result.value.path.begin() + queryIndex + 1,
                             result.value.path.end());

        for (const auto& part : params | std::views::split('&'))
        {
            std::string pair(part.begin(), part.end());

            auto equalsIndex = pair.find('=');
            if (equalsIndex != std::string::npos)
            {
                auto key   = trim(pair.substr(0, equalsIndex));
                auto value = trim(pair.substr(equalsIndex + 1));

                result.value.query[std::move(key)] = std::move(value);
            }
            else
            {
                result.value.query[pair] = "";
            }
        }

        result.value.path.resize(queryIndex);
    }

    std::string line;

    while (std::getline(requestStream, line) && requestStream.good())
    {
        if (auto colon = line.find(':'); colon != std::string::npos)
        {
            auto key = trim(line.substr(0, colon));

            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (result.value.headers.contains(key))
            {
                result.error      = "Duplicate headers are not allowed";
                result.statusCode = StatusCode::BadRequest;
                return result;
            }

            auto value                           = trim(line.substr(colon + 1));
            result.value.headers[std::move(key)] = std::move(value);
            continue;
        }

        if (result.value.headers.contains("content-length"))
        {
            auto contentLength =
                std::atoi(result.value.headers["content-length"].c_str());

            result.value.body = std::string(contentLength, '\0');
            contentLength =
                requestStream.readsome(result.value.body.data(), contentLength);
        }
        else if (!(line == "\r"))
        {
            result.error      = "Header folding is not allowed in HTTP/1.1";
            result.statusCode = StatusCode::BadRequest;
            return result;
        }
    }

    if (!result.value.headers.contains("host"))
    {
        result.error = "Missing Host header";
        return result;
    }

    result.success    = true;
    result.statusCode = StatusCode::OK;

    return std::move(result);
}

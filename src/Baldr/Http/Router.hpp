#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/Tuple.hpp>

namespace BALDR_NAMESPACE
{

    class SchemaRegistry;

    using RouteHandler = std::function<void(
        HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>)>;

    struct RouteEntry
    {
        std::regex               extractParamsRegex;
        std::vector<std::string> paramsNames = {};
        bool                     hasParams   = true;
        RouteHandler             handler;
        RouteOptions             options;
        std::string              groupPrefix;
        std::string              pathTemplate;
        HttpMethod               method { HttpMethod::Get };

        std::unordered_map<std::string, std::string> extractRouteParams(
            const std::string& path) const;
    };

    class Router
    {
      public:
        Router();

        ~Router();
        Router(const Router&)                = delete;
        Router& operator=(const Router&)     = delete;
        Router(Router&&) noexcept            = default;
        Router& operator=(Router&&) noexcept = default;

        template <typename Handler>
        void MapRoute(HttpMethod method, const std::string& route,
                      const std::string&  groupPrefix,
                      const RouteOptions& options, Handler&& handler)
        {
            insert(
                method,
                route,
                options,
                groupPrefix,
                [handler = std::forward<Handler>(handler)](
                    HttpRequest&  request,
                    HttpResponse& response,
                    const skr::Arc<skr::ServiceProvider>&
                        serviceProvider) mutable {
                    using HandlerArgsTuple = typename LambdaTraits<
                        std::remove_reference_t<decltype(handler)>>::ArgsTuple;

                    auto refFactory = [&]<typename TArg>(TArg* x) -> TArg& {
                        if constexpr (std::is_same_v<HttpRequest, TArg>)
                        {
                            return request;
                        }

                        if constexpr (std::is_same_v<HttpResponse, TArg>)
                        {
                            return response;
                        }

                        return *serviceProvider->GetService<TArg>();
                    };

                    auto ptrFactory = [&]<typename TArg>(
                                          skr::Arc<TArg>* x) -> skr::Arc<TArg> {
                        return serviceProvider
                            ->GetService<std::remove_pointer_t<TArg>>();
                    };

                    auto args = transformTuple<HandlerArgsTuple>(
                        refFactory, ptrFactory,
                        TupleOfPtr((HandlerArgsTuple*) nullptr));

                    using ResultType = LambdaResult<decltype(handler)>;
                    if constexpr (!std::is_same_v<ResultType, void>)
                    {
                        auto result = std::apply(handler, args);

                        if constexpr (std::is_base_of_v<IStreamingResult,
                                                        ResultType> &&
                                      !std::is_base_of_v<IResult, ResultType>)
                        {
                            response.streaming =
                                std::make_shared<ResultType>(std::move(result));
                        }
                        else if constexpr (std::is_base_of_v<IResult,
                                                             ResultType>)
                        {
                            result.Apply(response);
                        }
                        else if constexpr (std::is_same_v<const char*,
                                                          ResultType> ||
                                           std::is_same_v<char*, ResultType>)
                        {
                            response.headers["Content-Type"] = "text/plain";
                            response.body       = std::string(result);
                            response.statusCode = StatusCode::OK;
                        }
                        else if constexpr (std::is_assignable_v<std::string,
                                                                ResultType>)
                        {
                            response.headers["Content-Type"] = "text/plain";
                            response.body                    = result;
                            response.statusCode              = StatusCode::OK;
                        }
                        else
                        {
                            simdjson::simdjson_result<std::string> json =
                                simdjson::to_json(result);
                            if (json.has_value())
                            {
                                response.body = std::move(json).take_value();
                                response.headers["Content-Type"] =
                                    "application/json";
                                response.statusCode = StatusCode::OK;
                            }
                            else
                            {
                                response.headers["Content-Type"] = "text/plain";
                                response.body =
                                    "Handler returned a value that could not "
                                    "be "
                                    "serialized to JSON or std::string.";
                                response.statusCode =
                                    StatusCode::InternalServerError;
                            }
                        }
                    }
                    else
                    {
                        std::apply(handler, args);
                    }
                });
        }

        void insert(HttpMethod method, std::string path,
                    const RouteHandler& routeHandler) const;

        void insert(HttpMethod method, std::string path, RouteOptions options,
                    std::string         groupPrefix,
                    const RouteHandler& routeHandler) const;

        [[nodiscard]] std::optional<RouteEntry> match(HttpMethod,
                                                      std::string path) const;

        struct MatchResult
        {
            std::optional<RouteEntry> entry;
            std::vector<HttpMethod>   allowedMethodsOnPath;
            HttpMethod                resolvedMethod = HttpMethod::Get;
            std::string               routeTemplate;
        };

        [[nodiscard]] MatchResult matchWithAllow(HttpMethod  method,
                                                 std::string path) const;

        std::vector<RouteEntry> Snapshot() const;

        const skr::Arc<SchemaRegistry>& SchemaRegistrySlot() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace BALDR_NAMESPACE
#pragma once

#include <Skirnir/Skirnir.hpp>

#include <rfl.hpp>
#include <rfl/json.hpp>

#include "MiddlewareProvider.hpp"
#include "Router.hpp"
#include "Tuple.hpp"

class WebApplication : public skr::IApplication
{
  public:
    WebApplication(const Ref<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider),
        mRouter(rootServiceProvider->GetService<Router>()),
        mMiddlewareProvider(
            rootServiceProvider->GetService<MiddlewareProvider>())
    {
    }

    void MapGet(const std::string& route, auto&& handler)
    {
        MapRoute(HttpMethod::GET, route, handler);
    }

    void MapPost(const std::string& route, auto&& handler)
    {
        MapRoute(HttpMethod::POST, route, handler);
    }

    template <typename TMiddleware>
    const WebApplication& Use()
    {
        mMiddlewareProvider->AddMiddleware<TMiddleware>();

        return *this;
    }

    void Run() override;

  private:
    void MapRoute(HttpMethod method, const std::string& route, auto&& handler)
    {
        mRouter->insert(
            method,
            route,
            [&](HttpRequest& request, HttpResponse& response,
                Ref<skr::ServiceProvider> serviceProvider) {
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

                auto ptrFactory =
                    [&]<typename TArg>(Ref<TArg>* x) -> Ref<TArg> {
                    return serviceProvider
                        ->GetService<std::remove_pointer_t<TArg>>();
                };

                auto args = transformTuple<HandlerArgsTuple>(
                    refFactory, ptrFactory,
                    TupleOfPtr((HandlerArgsTuple*) nullptr));

                using ResultType = LambdaResult<decltype(handler)>;
                if constexpr (!std::is_same_v<ResultType, void>)
                {
                    auto result = std::move(std::apply(handler, args));

                    if constexpr (std::is_assignable_v<std::string, ResultType>)
                    {
                        response.headers["Content-Type"] = "plain/text";
                        response.body                    = result;
                    }
                    else
                    {
                        response.body = rfl::json::write(result);
                        response.headers["Content-Type"] = "application/json";
                    }

                    response.statusCode = StatusCode::OK;
                }
                else
                {
                    std::apply(handler, args);
                }
            });
    }

    Ref<Router>             mRouter;
    Ref<MiddlewareProvider> mMiddlewareProvider;
};

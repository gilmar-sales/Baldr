#pragma once

#include <meta>
#include <string>
#include <utility>

#include <Skirnir/Skirnir.hpp>

#include "HttpMethod.hpp"
#include "RouteOptions.hpp"
#include "Router.hpp"

#include "OpenApi/JsonSchemaEmitter.hpp"

#include "Result.hpp"

class WebApplication;

namespace Baldr
{
    class RouteRegistration
    {
      public:
        RouteRegistration(Router& router, HttpMethod method, std::string path,
                          std::string groupPrefix = "") :
            mRouter(router), mMethod(method), mPath(std::move(path)),
            mGroupPrefix(std::move(groupPrefix))
        {
        }

        RouteRegistration& WithSummary(std::string s)
        {
            mOptions.summary = std::move(s);
            return *this;
        }

        RouteRegistration& WithDescription(std::string d)
        {
            mOptions.description = std::move(d);
            return *this;
        }

        RouteRegistration& WithTag(std::string t)
        {
            mOptions.tags.push_back(std::move(t));
            return *this;
        }

        RouteRegistration& WithTags(std::vector<std::string> ts)
        {
            mOptions.tags = std::move(ts);
            return *this;
        }

        RouteRegistration& WithOperationId(std::string id)
        {
            mOptions.operationId = std::move(id);
            return *this;
        }

        RouteRegistration& WithDeprecated(bool v = true)
        {
            mOptions.deprecated = v;
            return *this;
        }

        RouteRegistration& WithConsumes(std::vector<std::string> mimes)
        {
            mOptions.consumes = std::move(mimes);
            return *this;
        }

        RouteRegistration& WithProduces(std::vector<std::string> mimes)
        {
            mOptions.produces = std::move(mimes);
            return *this;
        }

        RouteRegistration& WithMetadata(std::string key, std::string value)
        {
            mOptions.metadata[std::move(key)] = std::move(value);
            return *this;
        }

        // Stash a JSON Schema for either request or response body; consumed
        // by the OpenAPI extension. Strings are raw JSON Schema fragments.
        RouteRegistration& WithRequestSchemaJson(std::string schema)
        {
            mRequestSchemaJson = std::move(schema);
            return *this;
        }

        RouteRegistration& WithResponseSchemaJson(std::string schema)
        {
            mResponseSchemaJson = std::move(schema);
            return *this;
        }

        template <typename T>
        RouteRegistration& WithRequestType()
        {
            static_assert(
                IsAutoDerivable<T> || Detail::IsVectorOfAutoDerivableV<T>,
                "RouteRegistration::WithRequestType<T>: T must be a "
                "reflectable struct whose non-static data members are all "
                "in the supported primitive set (std::string, "
                "std::string_view, integral, double/float, bool).");

            auto& reg          = *mRouter.SchemaRegistrySlot();
            auto  ref          = TryEmitRefFor<T>(reg);
            mRequestSchemaJson = ref.value_or(std::string {});
            return *this;
        }

        template <typename T>
        RouteRegistration& WithResponseType()
        {
            static_assert(
                IsAutoDerivable<T> || Detail::IsVectorOfAutoDerivableV<T>,
                "RouteRegistration::WithRequestType<T>: T must be a "
                "reflectable struct whose non-static data members are all "
                "in the supported primitive set (std::string, "
                "std::string_view, integral, double/float, bool).");

            auto& reg           = *mRouter.SchemaRegistrySlot();
            auto  ref           = TryEmitRefFor<T>(reg);
            mResponseSchemaJson = ref.value_or(std::string {});
            return *this;
        }

        const RouteOptions& options() const { return mOptions; }
        const std::string&  requestSchemaJson() const
        {
            return mRequestSchemaJson;
        }
        const std::string& responseSchemaJson() const
        {
            return mResponseSchemaJson;
        }
        HttpMethod         method() const { return mMethod; }
        const std::string& path() const { return mPath; }
        const std::string& groupPrefix() const { return mGroupPrefix; }

        template <typename Handler>
        void Handle(Handler&& handler)
        {
            using ResultType = typename LambdaTraits<
                std::remove_reference_t<decltype(handler)>>::RetType;

            if (mResponseSchemaJson.empty())
            {
                constexpr bool isIResult =
                    std::is_base_of_v<IResult, ResultType> ||
                    std::is_base_of_v<IStreamingResult, ResultType>;

                if constexpr (!isIResult &&
                              (IsAutoDerivable<ResultType> ||
                               Detail::IsVectorOfAutoDerivableV<ResultType>) )
                {
                    auto& reg = *mRouter.SchemaRegistrySlot();
                    if (auto ref = TryEmitRefFor<ResultType>(reg))
                    {
                        mResponseSchemaJson = std::move(*ref);
                    }
                }
            }

            if (!mRequestSchemaJson.empty())
            {
                mOptions.metadata["requestSchemaJson"] = mRequestSchemaJson;
            }

            if (!mResponseSchemaJson.empty())
            {
                mOptions.metadata["responseSchemaJson"] = mResponseSchemaJson;
            }

            mRouter.MapRoute(mMethod, mPath, mGroupPrefix, mOptions,
                             std::forward<Handler>(handler));
        }

      private:
        Router&      mRouter;
        HttpMethod   mMethod;
        std::string  mPath;
        std::string  mGroupPrefix;
        RouteOptions mOptions;
        std::string  mRequestSchemaJson;
        std::string  mResponseSchemaJson;
        bool         mFinalised { false };
    };
} // namespace Baldr

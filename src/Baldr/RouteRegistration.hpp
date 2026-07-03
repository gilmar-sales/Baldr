#pragma once

#include <string>
#include <utility>

#include <Skirnir/Skirnir.hpp>

#include "HttpMethod.hpp"
#include "RouteOptions.hpp"
#include "Router.hpp"

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

        // Final binding step. Forwards to WebApplication::BindRoute with the
        // buffered options. Inline body requires WebApplication to be a
        // complete type at instantiation; call sites live in WebApplication
        // methods or after WebApplication.hpp has been included.
        template <typename Handler>
        void Handle(Handler&& handler)
        {
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
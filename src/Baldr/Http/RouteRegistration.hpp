/**
 * @file Http/RouteRegistration.hpp
 * @brief Fluent builder returned by the @c WebApplication::Map* methods.
 *        Collects OpenAPI metadata and the request/response schema before
 *        binding the handler to the router.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cstddef>
#include <meta>
#include <string>
#include <tuple>
#include <utility>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/FromParams.hpp>
#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/Router.hpp>

#include <Baldr/OpenApi/JsonSchemaEmitter.hpp>

#include <Baldr/Http/Results/Result.hpp>

namespace BALDR_NAMESPACE
{

    class WebApplication;

    /**
     * @brief Builder returned by @c WebApplication::Map* to attach OpenAPI
     *        metadata and a handler to a single route.
     *
     * Each @c With* method returns @c *this for chaining. The route is
     * registered when @ref Handle is called.
     */
    class RouteRegistration
    {
      public:
        /**
         * @brief Internal constructor used by the application and group
         *        builders. Not part of the user-facing API.
         */
        RouteRegistration(Router& router, HttpMethod method, std::string path,
                          std::string groupPrefix = "") :
            mRouter(router), mMethod(method), mPath(std::move(path)),
            mGroupPrefix(std::move(groupPrefix))
        {
        }

        /** @brief Set the OpenAPI operation summary. */
        RouteRegistration& WithSummary(std::string s)
        {
            mOptions.summary = std::move(s);
            return *this;
        }

        /** @brief Set the OpenAPI operation description. */
        RouteRegistration& WithDescription(std::string d)
        {
            mOptions.description = std::move(d);
            return *this;
        }

        /** @brief Append a single tag to the OpenAPI operation. */
        RouteRegistration& WithTag(std::string t)
        {
            mOptions.tags.push_back(std::move(t));
            return *this;
        }

        /** @brief Replace the OpenAPI operation tag list with @p ts. */
        RouteRegistration& WithTags(std::vector<std::string> ts)
        {
            mOptions.tags = std::move(ts);
            return *this;
        }

        /** @brief Set the OpenAPI @c operationId. */
        RouteRegistration& WithOperationId(std::string id)
        {
            mOptions.operationId = std::move(id);
            return *this;
        }

        /** @brief Mark the operation as deprecated (default @c true). */
        RouteRegistration& WithDeprecated(bool v = true)
        {
            mOptions.deprecated = v;
            return *this;
        }

        /** @brief Replace the OpenAPI @c consumes list. */
        RouteRegistration& WithConsumes(std::vector<std::string> mimes)
        {
            mOptions.consumes = std::move(mimes);
            return *this;
        }

        /** @brief Replace the OpenAPI @c produces list. */
        RouteRegistration& WithProduces(std::vector<std::string> mimes)
        {
            mOptions.produces = std::move(mimes);
            return *this;
        }

        /** @brief Add a free-form key/value pair to the metadata bag. */
        RouteRegistration& WithMetadata(std::string key, std::string value)
        {
            mOptions.metadata[std::move(key)] = std::move(value);
            return *this;
        }

        /**
         * @brief Stash a raw JSON Schema fragment describing the request
         *        body. Consumed verbatim by the OpenAPI extension.
         */
        RouteRegistration& WithRequestSchemaJson(std::string schema)
        {
            mRequestSchemaJson = std::move(schema);
            return *this;
        }

        /**
         * @brief Stash a raw JSON Schema fragment describing the response
         *        body. Consumed verbatim by the OpenAPI extension.
         */
        RouteRegistration& WithResponseSchemaJson(std::string schema)
        {
            mResponseSchemaJson = std::move(schema);
            return *this;
        }

        /**
         * @brief Derive the request schema from a reflectable C++ type @c T.
         *
         * The schema is emitted via @ref JsonSchemaEmitter and stored
         * under @c components.schemas; the route references it with @c $ref.
         *
         * @tparam T A reflectable struct whose non-static data members are
         *           all in the supported primitive set, or a
         *           @c std::vector of such a struct.
         */
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

        /**
         * @brief Derive the response schema from a reflectable C++ type @c T.
         *
         * @tparam T A reflectable struct (or @c std::vector of one) whose
         *           members are all in the supported primitive set.
         */
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

        /**
         * @brief Derive the @c parameters block (with @c in:"query") from
         *        a reflectable C++ type @c T.
         *
         * Every non-static data member of @c T is emitted as a required
         * query parameter. The schema for each field uses @ref
         * JsonSchemaEmitter's primitive mapping. The result is stashed
         * under @c mQueryParametersJson and forwarded into
         * @c RouteOptions::metadata at @ref Handle time.
         *
         * @tparam T A reflectable struct whose members are all in the
         *           supported primitive set.
         */
        template <typename T>
        RouteRegistration& WithQueryType()
        {
            static_assert(
                IsReflectableStruct<T>,
                "RouteRegistration::WithQueryType<T>: T must be a "
                "reflectable struct whose non-static data members are all "
                "in the supported primitive set (std::string, "
                "std::string_view, integral, double/float, bool).");
            mQueryParametersJson = emitParameterList<T>("query");
            return *this;
        }

        /**
         * @brief Derive the @c parameters block (with @c in:"path") from
         *        a reflectable C++ type @c T.
         *
         * Path parameters are always required. The schema for each field
         * uses @ref JsonSchemaEmitter's primitive mapping.
         *
         * @tparam T A reflectable struct whose members are all in the
         *           supported primitive set.
         */
        template <typename T>
        RouteRegistration& WithPathType()
        {
            static_assert(
                IsReflectableStruct<T>,
                "RouteRegistration::WithPathType<T>: T must be a "
                "reflectable struct whose non-static data members are all "
                "in the supported primitive set (std::string, "
                "std::string_view, integral, double/float, bool).");
            mPathParametersJson = emitParameterList<T>("path");
            return *this;
        }

        /// @return The accumulated OpenAPI options.
        const RouteOptions& options() const { return mOptions; }
        /// @return The raw JSON Schema string for the request body, if set.
        const std::string& requestSchemaJson() const
        {
            return mRequestSchemaJson;
        }
        /// @return The raw JSON Schema string for the response body, if set.
        const std::string& responseSchemaJson() const
        {
            return mResponseSchemaJson;
        }
        /// @return The raw JSON parameter array for query string, if set.
        const std::string& queryParametersJson() const
        {
            return mQueryParametersJson;
        }
        /// @return The raw JSON parameter array for path params, if set.
        const std::string& pathParametersJson() const
        {
            return mPathParametersJson;
        }
        /// @return The HTTP method this registration targets.
        HttpMethod method() const { return mMethod; }
        /// @return The path template (without group prefix).
        const std::string& path() const { return mPath; }
        /// @return The group prefix, or empty when not in a group.
        const std::string& groupPrefix() const { return mGroupPrefix; }

        /**
         * @brief Finalise the registration and bind @p handler.
         *
         * If the handler declares a @c FromBody<T>, @c FromQuery<T>, or
         * @c FromParams<T> parameter and no matching OpenAPI metadata was
         * supplied, the framework derives it automatically from @c T. If
         * the handler's return type is reflectable and no response schema
         * was supplied, the framework derives one automatically. The route
         * is inserted into the router and becomes dispatchable
         * immediately.
         */
        template <typename Handler>
        void Handle(Handler&& handler)
        {
            using ResultType = typename LambdaTraits<
                std::remove_reference_t<decltype(handler)>>::RetType;

            if (mRequestSchemaJson.empty())
            {
                using HandlerArgsTuple = typename LambdaTraits<
                    std::remove_reference_t<decltype(handler)>>::ArgsTuple;
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (DeriveRequestSchemaFromBody<I, HandlerArgsTuple>(), ...);
                }(std::make_index_sequence<
                    std::tuple_size_v<HandlerArgsTuple>> {});
            }

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

            if (!mQueryParametersJson.empty())
            {
                mOptions.metadata["queryParametersJson"] = mQueryParametersJson;
            }

            if (!mPathParametersJson.empty())
            {
                mOptions.metadata["pathParametersJson"] = mPathParametersJson;
            }

            mRouter.MapRoute(mMethod, mPath, mGroupPrefix, mOptions,
                             std::forward<Handler>(handler));
        }

        /// @brief If handler arg slot @c I is a pre-bound wrapper
        /// (@c FromBody<U>, @c FromQuery<U>, @c FromParams<U>), derive the
        /// corresponding OpenAPI metadata and stash it. No-op for other
        /// argument shapes or when metadata was already supplied.
        template <std::size_t I, typename HandlerArgsTuple>
        void DeriveRequestSchemaFromBody()
        {
            using Arg     = std::tuple_element_t<I, HandlerArgsTuple>;
            using BareArg = std::remove_cvref_t<Arg>;
            if constexpr (isFromBody_v<BareArg>)
            {
                using Payload = typename BareArg::ValueType;
                static_assert(
                    IsAutoDerivable<Payload> ||
                        Detail::IsVectorOfAutoDerivableV<Payload>,
                    "FromBody<T> payload type T must be reflectable with "
                    "auto-derivable non-static data members "
                    "(std::string, std::string_view, integral, "
                    "double/float, bool).");
                auto& reg = *mRouter.SchemaRegistrySlot();
                if (auto ref = TryEmitRefFor<Payload>(reg))
                {
                    mRequestSchemaJson = std::move(*ref);
                }
            }
            else if constexpr (isFromQuery_v<BareArg>)
            {
                using Payload = typename BareArg::ValueType;
                static_assert(
                    IsReflectableStruct<Payload>,
                    "FromQuery<T> payload type T must be reflectable with "
                    "auto-derivable non-static data members "
                    "(std::string, std::string_view, integral, "
                    "double/float, bool).");
                if (mQueryParametersJson.empty())
                    mQueryParametersJson = emitParameterList<Payload>("query");
            }
            else if constexpr (isFromParams_v<BareArg>)
            {
                using Payload = typename BareArg::ValueType;
                static_assert(
                    IsReflectableStruct<Payload>,
                    "FromParams<T> payload type T must be reflectable "
                    "with auto-derivable non-static data members "
                    "(std::string, std::string_view, integral, "
                    "double/float, bool).");
                if (mPathParametersJson.empty())
                    mPathParametersJson = emitParameterList<Payload>("path");
            }
        }

      private:
        /// @brief Emit a JSON @c parameters array walking reflectable
        /// members of @c T. Each member is rendered as a required
        /// parameter located at @p location ("query" or "path").
        template <typename T>
        std::string emitParameterList(std::string_view location)
        {
            static_assert(
                IsReflectableStruct<T>,
                "emitParameterList<T>: T must be a reflectable struct");
            std::string out;
            out += "[";
            bool first = true;
            template for (constexpr auto member : std::define_static_array(
                              std::meta::nonstatic_data_members_of(
                                  ^^T, std::meta::access_context::current())))
            {
                constexpr auto name = std::meta::identifier_of(member);
                T              obj {};
                using FieldT = std::remove_cvref_t<decltype(obj.[:member:])>;
                static_assert(
                    Detail::IsSupportedField<FieldT>::value,
                    "emitParameterList: member has an unsupported type "
                    "for auto-introspection");
                if (!first)
                    out += ",";
                first = false;
                out += "{\"name\":\"";
                out.append(name);
                out += "\",\"in\":\"";
                out.append(location);
                out += "\",\"required\":true,\"schema\":{\"type\":\"";
                out += Detail::PrimitiveTypeName<FieldT>();
                out += "\"}}";
            }
            out += "]";
            return out;
        }

        Router&      mRouter;
        HttpMethod   mMethod;
        std::string  mPath;
        std::string  mGroupPrefix;
        RouteOptions mOptions;
        std::string  mRequestSchemaJson;
        std::string  mResponseSchemaJson;
        std::string  mQueryParametersJson;
        std::string  mPathParametersJson;
        bool         mFinalised { false };
    };

} // namespace BALDR_NAMESPACE

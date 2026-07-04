/**
 * @file Http/Results/JsonBody.hpp
 * @brief JSON body deserialisation helpers built on simdjson and C++26
 *        reflection.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <simdjson.h>

#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    namespace detail
    {
        /// @brief Read a string field from a simdjson object into @p out.
        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name,
            std::string& out)
        {
            auto err = obj[name].get_string().get(out);

            return err;
        }

        /// @brief Read an @c int field (narrowed from @c int64_t).
        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name, int& out)
        {
            int64_t v   = 0;
            auto    err = obj[name].get_int64().get(v);
            if (!err)
                out = static_cast<int>(v);
            return err;
        }

        /// @brief Read an @c int64_t field.
        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name,
            int64_t& out)
        {
            return obj[name].get_int64().get(out);
        }

        /// @brief Read a @c double field.
        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name,
            double& out)
        {
            return obj[name].get_double().get(out);
        }

        /// @brief Read a @c bool field.
        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name, bool& out)
        {
            return obj[name].get_bool().get(out);
        }
    } // namespace detail

    /**
     * @brief Either-or result of a JSON body deserialisation.
     *
     * Holds either a value of @c T or a populated error response
     * (status + message) ready to be applied to an @ref HttpResponse.
     */
    template <typename T>
    class JsonBodyResult
    {
      public:
        JsonBodyResult() = default;

        /// @brief Construct a successful result wrapping @p value.
        static JsonBodyResult Ok(T value)
        {
            JsonBodyResult r;
            r.mValue = std::move(value);
            return r;
        }

        /// @brief Construct a failure result with @p status and @p message.
        static JsonBodyResult Fail(StatusCode status, std::string message)
        {
            JsonBodyResult r;
            r.mError.statusCode = status;
            r.mError.message    = std::move(message);
            return r;
        }

        /// @brief @c true when the parse succeeded.
        bool isOk() const { return mError.statusCode == StatusCode::OK; }

        /// @return Read-only access to the deserialised value.
        const T& value() const { return mValue; }
        /// @return Mutable access to the deserialised value.
        T&       value() { return mValue; }
        /// @brief Move the deserialised value out of the result.
        T        takeValue() { return std::move(mValue); }

        /// @brief Error payload carried by a failed parse.
        struct Error
        {
            StatusCode  statusCode = StatusCode::OK; ///< Suggested status code.
            std::string message;                     ///< Human-readable error.
        };
        /// @return The error payload.
        const Error& error() const { return mError; }

      private:
        T     mValue {};
        Error mError { StatusCode::OK, {} };
    };

    /**
     * @brief Parse the request body and return the top-level JSON object.
     *
     * Use this when you need to inspect fields manually; otherwise
     * @ref parseJson performs the deserialisation for you.
     *
     * @param request Incoming request.
     * @return The parsed object on success, or a failure with a 400
     *         status code and a human-readable message.
     */
    inline JsonBodyResult<simdjson::dom::object> parseJsonObject(
        const HttpRequest& request)
    {
        if (request.body.empty())
        {
            return JsonBodyResult<simdjson::dom::object>::Fail(
                StatusCode::BadRequest, "Empty request body");
        }

        static thread_local simdjson::dom::parser parser;
        simdjson::dom::element                    doc;
        auto err = parser.parse(request.body).get(doc);
        if (err)
        {
            return JsonBodyResult<simdjson::dom::object>::Fail(
                StatusCode::BadRequest,
                std::string("Invalid JSON: ") + simdjson::error_message(err));
        }

        simdjson::dom::object obj;
        err = doc.get_object().get(obj);
        if (err)
        {
            return JsonBodyResult<simdjson::dom::object>::Fail(
                StatusCode::BadRequest,
                "Expected a JSON object at the top level");
        }
        return JsonBodyResult<simdjson::dom::object>::Ok(std::move(obj));
    }

    /**
     * @brief Parse the request body and deserialise it into a @c T using
     *        C++26 reflection.
     *
     * Every non-static data member of @c T is read by name from the
     * top-level JSON object; missing or type-mismatched fields cause a
     * 400 response.
     *
     * Supported field types: @c std::string, @c std::string_view,
     * @c int, @c int64_t, @c double, @c bool. For richer types, either
     * provide specialisations of @c BALDR_NAMESPACE::detail::readJsonField
     * or fall back to @ref parseJsonObject and inspect the DOM yourself.
     *
     * @tparam T A reflectable struct whose members are all in the
     *           supported primitive set.
     * @param request Incoming request.
     */
    template <typename T>
    JsonBodyResult<T> parseJson(const HttpRequest& request)
    {
        auto obj = parseJsonObject(request);
        if (!obj.isOk())
        {
            return JsonBodyResult<T>::Fail(obj.error().statusCode,
                                           obj.error().message);
        }

        T     instance {};
        auto& o = obj.value();

        bool        anyError = false;
        std::string firstError;

        template for (constexpr auto member : std::define_static_array(
                          std::meta::nonstatic_data_members_of(
                              ^^T,
                              std::meta::access_context::current())))
        {
            if (anyError)
                break;
            constexpr auto name  = std::meta::identifier_of(member);
            auto&          field = instance.[:member:];
            using FieldT         = std::remove_cvref_t<decltype(field)>;

            simdjson::error_code err = simdjson::NO_SUCH_FIELD;
            if constexpr (std::is_same_v<FieldT, std::string>)
            {
                err = BALDR_NAMESPACE::detail::readJsonField(o, name, field);
            }
            else if constexpr (std::is_same_v<FieldT, std::string_view>)
            {
                std::string_view sv;
                err = o[name].get_string().get(sv);
                if (!err)
                    field = sv;
            }
            else if constexpr (std::is_same_v<FieldT, int> ||
                               std::is_same_v<FieldT, int64_t> ||
                               std::is_same_v<FieldT, double> ||
                               std::is_same_v<FieldT, bool>)
            {
                err = BALDR_NAMESPACE::detail::readJsonField(o, name, field);
            }
            else
            {
                firstError = "Unsupported field type for member '" +
                             std::string(name) + "'";
                anyError   = true;
                continue;
            }
            if (err)
            {
                firstError = "Field '" + std::string(name) +
                             "': " + simdjson::error_message(err);
                anyError   = true;
            }
        }

        if (anyError)
        {
            return JsonBodyResult<T>::Fail(StatusCode::BadRequest, firstError);
        }
        return JsonBodyResult<T>::Ok(std::move(instance));
    }

} // namespace BALDR_NAMESPACE

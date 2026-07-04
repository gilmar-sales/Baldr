#pragma once

#include <simdjson.h>

#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace Baldr
{
    namespace detail
    {
        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name,
            std::string& out)
        {
            auto err = obj[name].get_string().get(out);

            return err;
        }

        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name, int& out)
        {
            int64_t v   = 0;
            auto    err = obj[name].get_int64().get(v);
            if (!err)
                out = static_cast<int>(v);
            return err;
        }

        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name,
            int64_t& out)
        {
            return obj[name].get_int64().get(out);
        }

        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name,
            double& out)
        {
            return obj[name].get_double().get(out);
        }

        inline simdjson::error_code readJsonField(
            const simdjson::dom::object& obj, std::string_view name, bool& out)
        {
            return obj[name].get_bool().get(out);
        }
    } // namespace detail

    // Result of attempting to parse a JSON body. Either holds a value
    // of type `T` (deserialised from the top-level JSON object) or a
    // populated error response (status + message) ready to be applied
    // to a HttpResponse.
    template <typename T>
    class JsonResult
    {
      public:
        JsonResult() = default;

        static JsonResult Ok(T value)
        {
            JsonResult r;
            r.mValue = std::move(value);
            return r;
        }

        static JsonResult Fail(StatusCode status, std::string message)
        {
            JsonResult r;
            r.mError.statusCode = status;
            r.mError.message    = std::move(message);
            return r;
        }

        bool isOk() const { return mError.statusCode == StatusCode::OK; }

        const T& value() const { return mValue; }
        T&       value() { return mValue; }
        T        takeValue() { return std::move(mValue); }

        struct Error
        {
            StatusCode  statusCode = StatusCode::OK;
            std::string message;
        };
        const Error& error() const { return mError; }

      private:
        T     mValue {};
        Error mError { StatusCode::OK, {} };
    };

    // Parse the request body as JSON and return the top-level object.
    // Use this when you need to inspect fields manually; otherwise
    // `parseJson<T>` performs the deserialisation for you.
    inline JsonResult<simdjson::dom::object> parseJsonObject(
        const HttpRequest& request)
    {
        if (request.body.empty())
        {
            return JsonResult<simdjson::dom::object>::Fail(
                StatusCode::BadRequest, "Empty request body");
        }

        static thread_local simdjson::dom::parser parser;
        simdjson::dom::element                    doc;
        auto err = parser.parse(request.body).get(doc);
        if (err)
        {
            return JsonResult<simdjson::dom::object>::Fail(
                StatusCode::BadRequest,
                std::string("Invalid JSON: ") + simdjson::error_message(err));
        }

        simdjson::dom::object obj;
        err = doc.get_object().get(obj);
        if (err)
        {
            return JsonResult<simdjson::dom::object>::Fail(
                StatusCode::BadRequest,
                "Expected a JSON object at the top level");
        }
        return JsonResult<simdjson::dom::object>::Ok(std::move(obj));
    }

    // Parse the request body and deserialise it into a `T` using C++26
    // reflection. Every non-static data member of `T` is read by name
    // from the top-level JSON object; missing or type-mismatched fields
    // cause a 400 response.
    //
    // Supported field types: std::string, std::string_view, int,
    // int64_t, double, bool. For richer types, either provide
    // specialisations of `Baldr::detail::readJsonField` or fall back to
    // `parseJsonObject` and inspect the DOM yourself.
    template <typename T>
    JsonResult<T> parseJson(const HttpRequest& request)
    {
        auto obj = parseJsonObject(request);
        if (!obj.isOk())
        {
            return JsonResult<T>::Fail(obj.error().statusCode,
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
                err = Baldr::detail::readJsonField(o, name, field);
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
                err = Baldr::detail::readJsonField(o, name, field);
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
            return JsonResult<T>::Fail(StatusCode::BadRequest, firstError);
        }
        return JsonResult<T>::Ok(std::move(instance));
    }
} // namespace Baldr

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

        /**
         * @brief Translate a simdjson @c error_code into a custom
         *        human-readable message.
         *
         * Covers every value declared by @c simdjson::error_code so the
         * library never leaks simdjson's own wording into API responses.
         *
         * @param err The simdjson error code to translate.
         * @param field Optional JSON field name that triggered the error;
         *              included in the message when the error is field-local
         *              (e.g. missing/incorrect-type fields).
         * @return A descriptive, user-facing error string. @c SUCCESS maps
         *         to an empty string.
         */
        inline std::string simdjsonErrorMessage(simdjson::error_code err,
                                                std::string_view     field = {})
        {
            const std::string f =
                field.empty()
                    ? std::string {}
                    : std::string(" (Field '") + std::string(field) + "')";

            switch (err)
            {
                case simdjson::SUCCESS:
                    return {};
                case simdjson::CAPACITY:
                    return "JSON document is too large for the parser";
                case simdjson::MEMALLOC:
                    return "Out of memory while parsing JSON";
                case simdjson::TAPE_ERROR:
                    return "Internal parser error (tape)";
                case simdjson::DEPTH_ERROR:
                    return "JSON document exceeds the maximum nesting depth";
                case simdjson::STRING_ERROR:
                    return "Malformed string literal in JSON";
                case simdjson::T_ATOM_ERROR:
                    return "Malformed 'true' literal in JSON";
                case simdjson::F_ATOM_ERROR:
                    return "Malformed 'false' literal in JSON";
                case simdjson::N_ATOM_ERROR:
                    return "Malformed 'null' literal in JSON";
                case simdjson::NUMBER_ERROR:
                    return "Malformed number in JSON";
                case simdjson::BIGINT_ERROR:
                    return "Integer value exceeds 64-bit range" + f;
                case simdjson::UNINITIALIZED:
                    return "Uninitialised JSON document";
                case simdjson::EMPTY:
                    return "No JSON content was found";
                case simdjson::UNESCAPED_CHARS:
                    return "Unescaped characters in JSON string";
                case simdjson::UNCLOSED_STRING:
                    return "Unterminated string in JSON";
                case simdjson::UNSUPPORTED_ARCHITECTURE:
                    return "JSON parser does not support this CPU architecture";
                case simdjson::INCORRECT_TYPE:
                    return "JSON value has an incorrect type" + f;
                case simdjson::NUMBER_OUT_OF_RANGE:
                    return "JSON number is out of range" + f;
                case simdjson::INDEX_OUT_OF_BOUNDS:
                    return "JSON array index is out of bounds" + f;
                case simdjson::NO_SUCH_FIELD:
                    return "Missing required field" + f;
                case simdjson::IO_ERROR:
                    return "I/O error while reading JSON";
                case simdjson::INVALID_JSON_POINTER:
                    return "Invalid JSON pointer expression";
                case simdjson::INVALID_URI_FRAGMENT:
                    return "Invalid URI fragment";
                case simdjson::UNEXPECTED_ERROR:
                    return "Unexpected internal parser error";
                case simdjson::PARSER_IN_USE:
                    return "JSON parser is already in use";
                case simdjson::OUT_OF_ORDER_ITERATION:
                    return "Out-of-order iteration over JSON array or object";
                case simdjson::INSUFFICIENT_PADDING:
                    return "Insufficient padding in JSON input";
                case simdjson::INCOMPLETE_ARRAY_OR_OBJECT:
                    return "Incomplete JSON array or object";
                case simdjson::SCALAR_DOCUMENT_AS_VALUE:
                    return "Scalar JSON document cannot be used as a value";
                case simdjson::OUT_OF_BOUNDS:
                    return "Access outside of JSON document bounds";
                case simdjson::TRAILING_CONTENT:
                    return "Unexpected trailing content after JSON document";
                case simdjson::OUT_OF_CAPACITY:
                    return "JSON parser capacity exceeded";
                default:
                    return "Unknown JSON parsing error";
            }
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

        /// @brief Construct a failure result that also identifies the
        ///        offending field by name.
        static JsonBodyResult Fail(StatusCode status, std::string message,
                                   std::optional<std::string> field)
        {
            JsonBodyResult r;
            r.mError.statusCode = status;
            r.mError.message    = std::move(message);
            r.mError.field      = std::move(field);
            return r;
        }

        /// @brief @c true when the parse succeeded.
        bool isOk() const { return mError.statusCode == StatusCode::OK; }

        /// @return Read-only access to the deserialised value.
        const T& value() const { return mValue; }
        /// @return Mutable access to the deserialised value.
        T& value() { return mValue; }
        /// @brief Move the deserialised value out of the result.
        T takeValue() { return std::move(mValue); }

        /// @brief Error payload carried by a failed parse.
        struct Error
        {
            StatusCode  statusCode = StatusCode::OK; ///< Suggested status code.
            std::string message;                     ///< Human-readable error.
            std::optional<std::string>
                field; ///< First offending field, when one is known.
        };
        /// @return The error payload.
        const Error& error() const { return mError; }

      private:
        T     mValue {};
        Error mError { StatusCode::OK, {}, std::nullopt };
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
                "Invalid JSON: " + detail::simdjsonErrorMessage(err));
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

        bool                       anyError = false;
        std::string                firstError;
        std::optional<std::string> firstField;

        template for (constexpr auto member : std::define_static_array(
                          std::meta::nonstatic_data_members_of(
                              ^^T,
                              std::meta::access_context::current())))
        {
            if (anyError)
                break;

            constexpr auto name = std::meta::identifier_of(member);

            auto& field = instance.[:member:];

            using FieldT = std::remove_cvref_t<decltype(field)>;

            simdjson::error_code err = simdjson::NO_SUCH_FIELD;
            if constexpr (std::is_same_v<FieldT, std::string>)
            {
                err = BALDR_NAMESPACE::detail::readJsonField(o, name, field);
            }
            else if constexpr (std::is_same_v<FieldT, int> ||
                               std::is_same_v<FieldT, int64_t> ||
                               std::is_same_v<FieldT, double> ||
                               std::is_same_v<FieldT, bool>)
            {
                err = detail::readJsonField(o, name, field);
            }
            else
            {
                firstError = "Unsupported field type for member '" +
                             std::string(name) + "'";
                firstField = std::string(name);
                anyError   = true;

                continue;
            }

            if (err)
            {
                firstError = detail::simdjsonErrorMessage(err, name);

                firstField = std::string(name);

                anyError = true;
            }
        }

        if (anyError)
        {
            return JsonBodyResult<T>::Fail(
                StatusCode::BadRequest, firstError, std::move(firstField));
        }
        return JsonBodyResult<T>::Ok(std::move(instance));
    }

} // namespace BALDR_NAMESPACE

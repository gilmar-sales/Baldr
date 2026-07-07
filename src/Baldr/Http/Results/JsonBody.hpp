/**
 * @file Http/Results/JsonBody.hpp
 * @brief JSON body deserialisation helpers built on simdjson and C++26
 *        reflection.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <simdjson.h>

#include <array>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/StatusCode.hpp>
#include <Baldr/OpenApi/JsonSchemaEmitter.hpp>

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

        /**
         * @brief Status returned by the recursive JSON field deserialiser.
         *
         * Carries a @c simdjson @c error_code and the dotted/array-indexed
         * path of the offending JSON element. On success both fields are
         * empty.
         */
        struct JsonParseStatus
        {
            simdjson::error_code err = simdjson::SUCCESS;
            std::string          path;
        };

        /**
         * @brief Compose a child path by appending a member name to a
         *        parent path.
         *
         * @param parentPath Existing dotted path (empty for the root).
         * @param memberName Member to append.
         * @return @p parentPath when empty, otherwise
         *         @c parentPath + "." + memberName.
         */
        inline std::string joinMemberPath(std::string_view parentPath,
                                          std::string_view memberName)
        {
            std::string out;
            out.reserve(parentPath.size() + 1 + memberName.size());
            out.append(parentPath);
            if (!parentPath.empty())
                out += '.';
            out.append(memberName);
            return out;
        }

        /// @brief Compose a child path by appending an array index.
        inline std::string joinIndexPath(std::string_view parentPath,
                                         std::size_t      index)
        {
            std::string out;
            out.reserve(parentPath.size() + 12);
            out.append(parentPath);
            out += '[';
            out += std::to_string(index);
            out += ']';
            return out;
        }

        /// @brief @c true when @c T is a @c std::optional<U>.
        template <typename T>
        struct IsStdOptional : std::false_type
        {
        };

        template <typename U>
        struct IsStdOptional<std::optional<U>> : std::true_type
        {
        };

        /// @brief Convenience alias for @ref IsStdOptional.
        template <typename T>
        constexpr bool is_std_optional_v = IsStdOptional<T>::value;

        /// @brief @c true when @c T is a @c std::vector<U>.
        template <typename T>
        struct IsStdVector : std::false_type
        {
        };

        template <typename U>
        struct IsStdVector<std::vector<U>> : std::true_type
        {
        };

        /// @brief Convenience alias for @ref IsStdVector.
        template <typename T>
        constexpr bool is_std_vector_v = IsStdVector<T>::value;

        /// @brief @c true when @c T is a @c std::array<U, N>.
        template <typename T>
        struct IsStdArray : std::false_type
        {
        };

        template <typename U, std::size_t N>
        struct IsStdArray<std::array<U, N>> : std::true_type
        {
        };

        /// @brief Convenience alias for @ref IsStdArray.
        template <typename T>
        constexpr bool is_std_array_v = IsStdArray<T>::value;

        /**
         * @brief Recursively deserialise @p elem into @p out.
         *
         * Dispatches at compile time on the decayed type of @c FieldT
         * using @c if @c constexpr, supporting primitives,
         * @c std::optional, @c std::array, @c std::vector, and nested
         * reflectable structs (any class type satisfying
         * @ref BALDR_NAMESPACE::IsReflectableStruct ).
         *
         * @tparam FieldT Target C++ type.
         * @param elem    simdjson element to read.
         * @param path    Path of @p elem for error reporting.
         * @param out     Destination; left untouched on failure.
         * @return Empty status on success; populated status on failure
         *         (carrying the full dotted path of the failing element).
         */
        template <typename FieldT>
        JsonParseStatus ReadField(const simdjson::dom::element& elem,
                                  std::string_view              path,
                                  FieldT&                       out)
        {
            if constexpr (std::is_same_v<FieldT, std::string>)
            {
                auto err = elem.get_string().get(out);
                if (err)
                    return { err, std::string(path) };
                return {};
            }
            else if constexpr (std::is_same_v<FieldT, std::string_view>)
            {
                std::string_view sv;
                auto             err = elem.get_string().get(sv);
                if (err)
                    return { err, std::string(path) };
                out = sv;
                return {};
            }
            else if constexpr (std::is_same_v<FieldT, int>)
            {
                int64_t v   = 0;
                auto    err = elem.get_int64().get(v);
                if (err)
                    return { err, std::string(path) };
                out = static_cast<int>(v);
                return {};
            }
            else if constexpr (std::is_same_v<FieldT, int64_t>)
            {
                auto err = elem.get_int64().get(out);
                if (err)
                    return { err, std::string(path) };
                return {};
            }
            else if constexpr (std::is_same_v<FieldT, double> ||
                               std::is_same_v<FieldT, float>)
            {
                double v   = 0.0;
                auto   err = elem.get_double().get(v);
                if (err)
                    return { err, std::string(path) };
                out = static_cast<FieldT>(v);
                return {};
            }
            else if constexpr (std::is_same_v<FieldT, bool>)
            {
                auto err = elem.get_bool().get(out);
                if (err)
                    return { err, std::string(path) };
                return {};
            }
            else if constexpr (BALDR_NAMESPACE::IsReflectableStruct<FieldT>)
            {
                simdjson::dom::object child;
                auto                  err = elem.get_object().get(child);
                if (err)
                    return { err, std::string(path) };

                template for (constexpr auto member : std::define_static_array(
                                  std::meta::nonstatic_data_members_of(
                                      ^^FieldT,
                                      std::meta::access_context::current())))
                {
                    constexpr auto memberName =
                        std::meta::identifier_of(member);
                    using InnerT =
                        std::remove_cvref_t<decltype(out.[:member:])>;

                    auto fieldErr = child[memberName].error();
                    if (fieldErr == simdjson::NO_SUCH_FIELD)
                    {
                        if constexpr (detail::is_std_optional_v<InnerT>)
                        {
                            out.[:member:] = std::nullopt;
                            continue;
                        }
                        else if constexpr (detail::is_std_vector_v<InnerT>)
                        {
                            out.[:member:].clear();
                            continue;
                        }
                        else if constexpr (BALDR_NAMESPACE::IsReflectableStruct<
                                               InnerT> ||
                                           detail::is_std_array_v<InnerT>)
                        {
                            continue;
                        }
                        else
                        {
                            return { simdjson::NO_SUCH_FIELD,
                                     detail::joinMemberPath(path, memberName) };
                        }
                    }

                    simdjson::dom::element e;
                    if (child[memberName].get(e))
                    {
                        return { fieldErr,
                                 detail::joinMemberPath(path, memberName) };
                    }
                    auto childPath = detail::joinMemberPath(path, memberName);
                    auto s =
                        detail::ReadField<InnerT>(e, childPath, out.[:member:]);
                    if (s.err)
                        return s;
                }
                return {};
            }
            else if constexpr (detail::is_std_optional_v<FieldT>)
            {
                using InnerT = typename FieldT::value_type;
                if (elem.type() == simdjson::dom::element_type::NULL_VALUE)
                {
                    out = std::nullopt;
                    return {};
                }
                InnerT inner {};
                auto   status = ReadField<InnerT>(elem, path, inner);
                if (status.err)
                    return status;
                out = std::move(inner);
                return {};
            }
            else if constexpr (detail::is_std_array_v<FieldT>)
            {
                simdjson::dom::array arr;
                auto                 err = elem.get_array().get(arr);
                if (err)
                    return { err, std::string(path) };

                using InnerT            = typename FieldT::value_type;
                constexpr std::size_t N = std::tuple_size_v<FieldT>;

                std::size_t i = 0;
                for (auto child : arr)
                {
                    if (i >= N)
                        return { simdjson::INDEX_OUT_OF_BOUNDS,
                                 detail::joinIndexPath(path, i) };
                    auto   childPath = detail::joinIndexPath(path, i);
                    InnerT item {};
                    auto   status = ReadField<InnerT>(child, childPath, item);
                    if (status.err)
                        return status;
                    out[i] = item;
                    ++i;
                }
                if (i != N)
                    return { simdjson::INDEX_OUT_OF_BOUNDS,
                             detail::joinIndexPath(path, i) };
                return {};
            }
            else if constexpr (is_std_vector_v<FieldT>)
            {
                simdjson::dom::array arr;
                auto                 err = elem.get_array().get(arr);
                if (err)
                    return { err, std::string(path) };

                using InnerT = typename FieldT::value_type;

                out.clear();
                std::size_t i = 0;
                for (auto child : arr)
                {
                    auto   childPath = detail::joinIndexPath(path, i);
                    InnerT inner {};
                    auto   status = ReadField<InnerT>(child, childPath, inner);
                    if (status.err)
                        return status;
                    out.push_back(std::move(inner));
                    ++i;
                }
                return {};
            }
            else
            {
                return { simdjson::INCORRECT_TYPE, std::string(path) };
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
     * top-level JSON object. Missing required fields and type-mismatched
     * fields produce a 400 response with a dotted path in @c Error::field
     * (e.g. @c "address.city").
     *
     * Supported field types:
     * - Primitives: @c std::string, @c std::string_view, any integral
     *   type, @c double, @c float, @c bool.
     * - @c std::optional<U> where @c U is itself supported (missing or
     *   explicit JSON @c null both leave the field as @c std::nullopt).
     * - @c std::array<U, N> of a supported element type.
     * - @c std::vector<U> of a supported element type.
     * - Nested reflectable structs (recursively).
     *
     * @c std::string_view fields bind to the simdjson document's internal
     * string storage; the parsed @c HttpRequest must outlive the
     * returned @c JsonBodyResult.
     *
     * For richer types, fall back to @ref parseJsonObject and inspect
     * the DOM yourself.
     *
     * @tparam T A reflectable struct whose members are all supported.
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

        detail::JsonParseStatus status {};

        template for (constexpr auto member : std::define_static_array(
                          std::meta::nonstatic_data_members_of(
                              ^^T,
                              std::meta::access_context::current())))
        {
            constexpr auto memberName = std::meta::identifier_of(member);
            using FieldT = std::remove_cvref_t<decltype(instance.[:member:])>;

            if constexpr (detail::is_std_optional_v<FieldT>)
            {
                if (o[memberName].error() == simdjson::NO_SUCH_FIELD)
                {
                    instance.[:member:] = std::nullopt;
                    continue;
                }
            }
            else if constexpr (detail::is_std_vector_v<FieldT>)
            {
                if (o[memberName].error() == simdjson::NO_SUCH_FIELD)
                {
                    instance.[:member:].clear();
                    continue;
                }
            }
            else if constexpr (BALDR_NAMESPACE::IsReflectableStruct<FieldT> ||
                               detail::is_std_array_v<FieldT>)
            {
                if (o[memberName].error() == simdjson::NO_SUCH_FIELD)
                {
                    continue;
                }
            }

            auto childPath = detail::joinMemberPath({}, memberName);
            simdjson::dom::element e;
            auto                   lookupErr = o[memberName].get(e);
            if (lookupErr)
            {
                status = { lookupErr, childPath };
                break;
            }
            auto s =
                detail::ReadField<FieldT>(e, childPath, instance.[:member:]);
            if (s.err)
            {
                status = s;
                break;
            }
        }

        if (status.err)
        {
            std::string msg =
                detail::simdjsonErrorMessage(status.err, status.path);
            return JsonBodyResult<T>::Fail(
                StatusCode::BadRequest, msg, status.path);
        }
        return JsonBodyResult<T>::Ok(std::move(instance));
    }

} // namespace BALDR_NAMESPACE

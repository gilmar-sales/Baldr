/**
 * @file Http/Results/TypedResults.hpp
 * @brief Status-specific @ref IResult subclasses whose return types feed the
 *        OpenAPI generator one entry per status code.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/StatusCode.hpp>
#include <Baldr/OpenApi/JsonSchemaEmitter.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Base class for semantic result types (OkResult, NotFoundResult,
     *        BadRequestResult, ...).
     *
     * Subclasses fix the HTTP status code and the default body shape (string
     * vs. JSON vs. empty) and expose those facts to the OpenAPI generator so
     * each status code becomes its own entry in the operation's
     * @c responses map.
     */
    class TypedResult : public IResult
    {
      public:
        /**
         * @brief HTTP status code that this result writes to the response.
         */
        [[nodiscard]] virtual StatusCode StatusFor() const = 0;

        /**
         * @brief Content-Type header value the result applies to the
         *        response, or empty when no body is written.
         */
        [[nodiscard]] virtual std::string_view ContentTypeFor() const = 0;

        /**
         * @brief Raw JSON Schema fragment describing the response body, or
         *        @c "{}" when the result has no body (e.g. 204).
         */
        [[nodiscard]] virtual std::string_view SchemaJsonFor() const
        {
            return "{\"type\":\"string\"}";
        }

        /**
         * @brief Default @c Apply implementation. Writes the status code,
         *        body and Content-Type header derived from the virtual
         *        accessors and the optional body string.
         */
        void Apply(HttpResponse& response) const override
        {
            auto body = BodyFor();
            if (!body.empty())
            {
                response.body = std::move(body);
            }
            if (!ContentTypeFor().empty())
            {
                response.headers["Content-Type"] =
                    std::string(ContentTypeFor());
            }
            response.statusCode = StatusFor();
        }

        /**
         * @brief Default body string. Subclasses override or set the body
         *        via constructor; the default returns empty (e.g. 204).
         */
        [[nodiscard]] virtual std::string BodyFor() const { return {}; }
    };

    /**
     * @brief CRTP base for typed results with a default text body. Provides
     *        @c StatusCodeV and @c DefaultSchemaV as static constants so
     *        route registration can derive per-status metadata at compile
     *        time without an instance.
     *
     * @tparam Derived The concrete result subclass.
     * @tparam Status  The HTTP status code this subclass writes.
     */
    template <typename Derived, StatusCode Status>
    class StringBodyTypedResult : public TypedResult
    {
      public:
        static constexpr StatusCode StatusCodeV = Status;
        static constexpr const char* DefaultSchemaV = "{\"type\":\"string\"}";
    };

    /**
     * @brief Convenience base for body-less typed results (204). Provides a
     *        schema of @c "{}" and an empty Content-Type.
     */
    template <typename Derived, StatusCode Status>
    class EmptyBodyTypedResult : public TypedResult
    {
      public:
        static constexpr StatusCode StatusCodeV = Status;
        static constexpr const char* DefaultSchemaV = "{}";

        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return {};
        }
        [[nodiscard]] std::string_view SchemaJsonFor() const override
        {
            return "{}";
        }
    };

    /**
     * @brief 200 OK with a string or JSON body.
     *
     * Use @ref Text for plain text and @ref Json for a reflectable payload
     * type.
     */
    class OkResult final
        : public StringBodyTypedResult<OkResult, StatusCode::OK>
    {
      public:
        /// @brief Construct a 200 OK response with a text body.
        explicit OkResult(std::string body) : mBody(std::move(body)) {}

        /// @brief Construct a 200 OK response with a custom body and schema
        /// (used internally by @ref Json).
        OkResult(std::string body, std::string) : mBody(std::move(body)) {}

        /**
         * @brief Build an OkResult carrying a serialised JSON payload of a
         *        reflectable C++ type @c T.
         */
        template <typename T>
        [[nodiscard]] static OkResult Json(
            const T& value, SchemaRegistry* reg = nullptr)
        {
            (void) reg;
            return OkResult(simdjson::to_json_string(value), std::string());
        }

        /// @brief Build a plain-text OkResult.
        [[nodiscard]] static OkResult Text(std::string body)
        {
            return OkResult(std::move(body));
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::OK;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 201 Created with a text body.
    class CreatedResult final
        : public StringBodyTypedResult<CreatedResult, StatusCode::Created>
    {
      public:
        explicit CreatedResult(std::string body = "Created") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::Created;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 204 No Content — no body, no Content-Type.
    class NoContentResult final
        : public EmptyBodyTypedResult<NoContentResult, StatusCode::NoContent>
    {
      public:
        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::NoContent;
        }
    };

    /// @brief 400 Bad Request with an optional text body describing why.
    class BadRequestResult final
        : public StringBodyTypedResult<BadRequestResult, StatusCode::BadRequest>
    {
      public:
        explicit BadRequestResult(std::string body = "Bad Request") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::BadRequest;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 401 Unauthorized with an optional text body.
    class UnauthorizedResult final
        : public StringBodyTypedResult<UnauthorizedResult,
                                      StatusCode::Unauthorized>
    {
      public:
        explicit UnauthorizedResult(std::string body = "Unauthorized") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::Unauthorized;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 403 Forbidden with an optional text body.
    class ForbiddenResult final
        : public StringBodyTypedResult<ForbiddenResult, StatusCode::Forbidden>
    {
      public:
        explicit ForbiddenResult(std::string body = "Forbidden") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::Forbidden;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 404 Not Found with an optional text body.
    class NotFoundResult final
        : public StringBodyTypedResult<NotFoundResult, StatusCode::NotFound>
    {
      public:
        explicit NotFoundResult(std::string body = "Not Found") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::NotFound;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 409 Conflict with an optional text body.
    class ConflictResult final
        : public StringBodyTypedResult<ConflictResult, StatusCode::Conflict>
    {
      public:
        explicit ConflictResult(std::string body = "Conflict") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::Conflict;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 422 Unprocessable Entity with an optional text body.
    class UnprocessableEntityResult final
        : public StringBodyTypedResult<UnprocessableEntityResult,
                                      StatusCode::UnprocessableEntity>
    {
      public:
        explicit UnprocessableEntityResult(std::string body =
                                               "Unprocessable Entity") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::UnprocessableEntity;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /// @brief 500 Internal Server Error with an optional text body.
    class InternalServerErrorResult final
        : public StringBodyTypedResult<InternalServerErrorResult,
                                      StatusCode::InternalServerError>
    {
      public:
        explicit InternalServerErrorResult(std::string body =
                                               "Internal Server Error") :
            mBody(std::move(body))
        {
        }

        [[nodiscard]] StatusCode StatusFor() const override
        {
            return StatusCode::InternalServerError;
        }
        [[nodiscard]] std::string_view ContentTypeFor() const override
        {
            return "text/plain";
        }
        [[nodiscard]] std::string BodyFor() const override { return mBody; }

      private:
        std::string mBody;
    };

    /**
     * @brief @c true when @c T derives from @ref TypedResult so the route
     *        registration can take the multi-status code path.
     */
    template <typename T>
    inline constexpr bool IsTypedResultV =
        std::is_base_of_v<TypedResult, std::remove_cvref_t<T>>;

} // namespace BALDR_NAMESPACE
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/Results/TypedResults.hpp>
#include <Baldr/Http/StatusCode.hpp>

#include <gtest/gtest.h>

#include <string>

#include "UserDto.hpp"

#include <string>

struct ValidationError
{
    std::string field;
    std::string message;
};

TEST(TypedJsonResultSpec, ApplyWritesStatusAndJsonBody)
{
    baldr::JsonResult<UserDto, baldr::StatusCode::OK> r { UserDto {
        "Ada", 36 } };

    baldr::HttpResponse response;
    r.Apply(response);

    EXPECT_EQ(response.statusCode, baldr::StatusCode::OK);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(response.body, R"({"name":"Ada","age":36})");
}

TEST(TypedJsonResultSpec, StatusForMatchesTemplateParameter)
{
    baldr::JsonResult<UserDto, baldr::StatusCode::Conflict> r { UserDto {
        "bob", 1 } };
    EXPECT_EQ(r.StatusFor(), baldr::StatusCode::Conflict);
    EXPECT_EQ(r.ContentTypeFor(), "application/json");
}

TEST(TypedJsonResultSpec, SchemaFragmentRegistersTypeAndReturnsRef)
{
    baldr::SchemaRegistry reg;
    std::string fragment = baldr::TypedJsonResultSchemaFragment<UserDto>(reg);
    EXPECT_EQ(fragment, R"({"$ref":"#/components/schemas/UserDto"})");
    EXPECT_TRUE(reg.Contains("UserDto"));
}

TEST(TypedJsonResultSpec, ResultsFactoryReturnsTypedJson)
{
    auto r = baldr::Results::Json<UserDto, baldr::StatusCode::BadRequest>(
        UserDto { "x", 0 });
    static_assert(
        baldr::IsTypedJsonResultV<decltype(r)>,
        "Results::Json<T, Status> must return a TypedJsonResult<T, Status>");

    baldr::HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(response.statusCode, baldr::StatusCode::BadRequest);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST(TypedJsonResultSpec, IsTypedJsonResultVFalseForUnrelated)
{
    static_assert(!baldr::IsTypedJsonResultV<int>, "");
    static_assert(!baldr::IsTypedJsonResultV<baldr::JsonResult>, "");
    static_assert(!baldr::IsTypedJsonResultV<baldr::OkResult>, "");
}

TEST(TypedJsonResultSpec, IsTypedJsonResultVTrueForSpecialisation)
{
    static_assert(baldr::IsTypedJsonResultV<
                      baldr::JsonResult<UserDto, baldr::StatusCode::OK>>,
                  "");
}

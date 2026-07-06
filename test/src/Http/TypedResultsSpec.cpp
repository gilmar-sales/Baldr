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

TEST(JsonResultSpec, ApplyWritesStatusAndJsonBody)
{
    baldr::JsonResult<UserDto, baldr::StatusCode::OK> r { UserDto {
        "Ada", 36 } };

    baldr::HttpResponse response;
    r.Apply(response);

    EXPECT_EQ(response.statusCode, baldr::StatusCode::OK);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(response.body, R"({"name":"Ada","age":36})");
}

TEST(JsonResultSpec, StatusForMatchesTemplateParameter)
{
    baldr::JsonResult<UserDto, baldr::StatusCode::Conflict> r { UserDto {
        "bob", 1 } };
    EXPECT_EQ(r.StatusFor(), baldr::StatusCode::Conflict);
    EXPECT_EQ(r.ContentTypeFor(), "application/json");
}

TEST(JsonResultSpec, SchemaFragmentRegistersTypeAndReturnsRef)
{
    baldr::SchemaRegistry reg;
    std::string fragment = baldr::JsonResultSchemaFragment<UserDto>(reg);
    EXPECT_EQ(fragment, R"({"$ref":"#/components/schemas/UserDto"})");
    EXPECT_TRUE(reg.Contains("UserDto"));
}

TEST(JsonResultSpec, ResultsFactoryReturnsTypedJson)
{
    auto r = baldr::Results::Json<UserDto, baldr::StatusCode::BadRequest>(
        UserDto { "x", 0 });
    static_assert(
        baldr::IsJsonResultV<decltype(r)>,
        "Results::Json<T, Status> must return a JsonResult<T, Status>");

    baldr::HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(response.statusCode, baldr::StatusCode::BadRequest);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST(JsonResultSpec, IsJsonResultVFalseForUnrelated)
{
    static_assert(!baldr::IsJsonResultV<int>, "");
    static_assert(!baldr::IsJsonResultV<baldr::OkResult>, "");
}

TEST(JsonResultSpec, IsJsonResultVTrueForSpecialisation)
{
    static_assert(
        baldr::IsJsonResultV<baldr::JsonResult<UserDto, baldr::StatusCode::OK>>,
        "");
}

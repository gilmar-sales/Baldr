#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/Results/ResultDispatch.hpp>
#include <Baldr/Http/StatusCode.hpp>

#include <string>
#include <variant>

namespace
{
    struct Product
    {
        std::string name;
        int         price;
    };

    struct ProductDto
    {
        std::string sku;
        int         stock;
    };
} // namespace

class VariantResultSpec : public ::testing::Test
{
};

TEST_F(VariantResultSpec, VariantOfIResultsDispatchesActiveAlternative)
{
    using V =
        std::variant<baldr::TextResult,
                     baldr::JsonResult<ProductDto, baldr::StatusCode::OK>>;

    baldr::HttpResponse response;

    baldr::detail::ApplyHandlerResult(V(std::in_place_index<0>, "missing"),
                                      response);
    EXPECT_EQ(response.body, "missing");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");

    response = baldr::HttpResponse();
    baldr::detail::ApplyHandlerResult(
        V(std::in_place_index<1>, ProductDto { "a", 1 }),
        response);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::OK));
}

TEST_F(VariantResultSpec, VariantMixingJsonAndNotFound)
{
    using V = std::variant<baldr::JsonResult<ProductDto, baldr::StatusCode::OK>,
                           baldr::StatusResult>;

    baldr::HttpResponse response;

    baldr::detail::ApplyHandlerResult(
        V(std::in_place_index<0>, ProductDto { "x", 0 }),
        response);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::OK));

    response = baldr::HttpResponse();
    baldr::detail::ApplyHandlerResult(
        V(std::in_place_index<1>, baldr::StatusCode::NotFound),
        response);
    EXPECT_TRUE(response.body.empty());
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::NotFound));
}

TEST_F(VariantResultSpec, VariantWithMonostateProducesEmptyOk)
{
    using V = std::variant<std::monostate, baldr::StatusResult>;

    baldr::HttpResponse response;

    baldr::detail::ApplyHandlerResult(V(std::in_place_index<0>), response);
    EXPECT_TRUE(response.body.empty());
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::OK));

    response = baldr::HttpResponse();
    baldr::detail::ApplyHandlerResult(
        V(std::in_place_index<1>, baldr::StatusCode::Accepted),
        response);
    EXPECT_TRUE(response.body.empty());
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::Accepted));
}

TEST_F(VariantResultSpec, VariantWithAssignableToStringUsesTextPlain)
{
    using V = std::variant<int, std::string>;

    baldr::HttpResponse response;

    baldr::detail::ApplyHandlerResult(V(std::in_place_index<0>, 65), response);
    EXPECT_EQ(response.body, "A");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");

    response = baldr::HttpResponse();
    baldr::detail::ApplyHandlerResult(
        V(std::in_place_index<1>, std::string("hi")),
        response);
    EXPECT_EQ(response.body, "hi");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");
}

TEST_F(VariantResultSpec, VariantIsStdTraitDetectsStdVariant)
{
    static_assert(
        baldr::detail::is_std_variant_v<std::variant<int, std::string>>);
    static_assert(!baldr::detail::is_std_variant_v<int>);
    static_assert(!baldr::detail::is_std_variant_v<std::string>);
}
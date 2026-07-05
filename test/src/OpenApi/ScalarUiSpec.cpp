#include <Baldr/OpenApi/MapScalarUi.hpp>

#include <cstddef>
#include <string_view>

#include <gtest/gtest.h>

TEST(ScalarUiSpec, EmbeddedScalarAssetBytesAreNonEmpty)
{
    using namespace baldr;

    EXPECT_GT(OpenApi::EmbeddedScalar::kScalarReferenceJsSize, 0u);
    EXPECT_GT(OpenApi::EmbeddedScalar::kStylesCssSize, 0u);
    EXPECT_GT(OpenApi::EmbeddedScalar::kIndexHtmlSize, 0u);
}

TEST(ScalarUiSpec, EmbeddedScalarJsLooksLikeJsBundle)
{
    using namespace baldr;
    auto view =
        OpenApi::EmbeddedScalar::AsStringView(OpenApi::EmbeddedScalar::kScalarReferenceJs);
    // Scalar's standalone bundle starts with a JSDoc-style comment block
    // (Terser preserves the original `/**` for the licence header).
    ASSERT_GE(view.size(), 2u);
    EXPECT_EQ(view.substr(0, 2), std::string_view("/*"));
}

TEST(ScalarUiSpec, EmbeddedScalarCssLooksLikeCssBundle)
{
    using namespace baldr;
    auto view =
        OpenApi::EmbeddedScalar::AsStringView(OpenApi::EmbeddedScalar::kStylesCss);
    // The vendored Scalar stylesheet starts with a JSDoc-style licence
    // header, similar to the JS bundle.
    ASSERT_GE(view.size(), 2u);
    EXPECT_EQ(view.substr(0, 2), std::string_view("/*"));
}

TEST(ScalarUiSpec, EmbeddedScalarIndexHtmlIsServeableAndCarriesPlaceholders)
{
    using namespace baldr;
    auto view =
        OpenApi::EmbeddedScalar::AsStringView(OpenApi::EmbeddedScalar::kIndexHtml);
    EXPECT_NE(view.find("__TITLE__"), std::string_view::npos);
    EXPECT_NE(view.find("__SPEC_URL__"), std::string_view::npos);
    EXPECT_NE(view.find("__JS_URL__"), std::string_view::npos);
    EXPECT_NE(view.find("__STYLES_URL__"), std::string_view::npos);
}

TEST(ScalarUiSpec, AsStringViewSizeMatchesUnderlyingArray)
{
    using namespace baldr;
    EXPECT_EQ(OpenApi::EmbeddedScalar::AsStringView(
                  OpenApi::EmbeddedScalar::kScalarReferenceJs)
                  .size(),
              OpenApi::EmbeddedScalar::kScalarReferenceJsSize);
    EXPECT_EQ(OpenApi::EmbeddedScalar::AsStringView(
                  OpenApi::EmbeddedScalar::kStylesCss)
                  .size(),
              OpenApi::EmbeddedScalar::kStylesCssSize);
    EXPECT_EQ(OpenApi::EmbeddedScalar::AsStringView(
                  OpenApi::EmbeddedScalar::kIndexHtml)
                  .size(),
              OpenApi::EmbeddedScalar::kIndexHtmlSize);
}

TEST(ScalarUiSpec, ScalarUiLoggerTagExists)
{
    using namespace baldr;
    static_assert(std::is_same_v<ScalarUi, ScalarUi>,
                  "ScalarUi tag must be a complete type");
    EXPECT_NO_THROW(static_cast<void>(typeid(ScalarUi)));
}
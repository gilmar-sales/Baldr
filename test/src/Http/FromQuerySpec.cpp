#include <Baldr/Http/FromQuery.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
    struct SearchFilters
    {
        std::string name;
        int         age    = 0;
        bool        active = false;
        double      weight = 0.0;
    };

    struct AllSupported
    {
        std::string      s;
        std::string_view sv;
        int              i;
        int64_t          i64;
        double           d;
        float            f;
        bool             b;
    };

    struct Unsupported
    {
        int   id;
        void* raw;
    };
} // namespace

TEST(FromQueryTest, ParsesValidQueryParametersIntoShell)
{
    baldr::HttpRequest request;
    request.query["name"]   = std::string("Alice");
    request.query["age"]    = std::string("30");
    request.query["active"] = std::string("true");
    request.query["weight"] = std::string("62.5");

    auto bound = baldr::detail::bindFromQuery<SearchFilters>(request);

    EXPECT_TRUE(bound.isOk());
    EXPECT_FALSE(bound.error.has_value());
    EXPECT_EQ(bound.value.name, "Alice");
    EXPECT_EQ(bound.value.age, 30);
    EXPECT_TRUE(bound.value.active);
    EXPECT_DOUBLE_EQ(bound.value.weight, 62.5);
}

TEST(FromQueryTest, ReportsFieldMissingFromQuery)
{
    baldr::HttpRequest request;
    request.query["name"]   = std::string("Bob");
    request.query["active"] = std::string("false");
    // "age" + "weight" missing

    auto bound = baldr::detail::bindFromQuery<SearchFilters>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_EQ(static_cast<int>(bound.error->statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_NE(bound.error->message.find("query"), std::string::npos);
}

TEST(FromQueryTest, ReportsNumericParseFailure)
{
    baldr::HttpRequest request;
    request.query["name"]   = std::string("Alice");
    request.query["age"]    = std::string("not-a-number");
    request.query["active"] = std::string("true");
    request.query["weight"] = std::string("1.0");

    auto bound = baldr::detail::bindFromQuery<SearchFilters>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_EQ(static_cast<int>(bound.error->statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_NE(bound.error->message.find("could not be parsed"),
              std::string::npos);
}

TEST(FromQueryTest, AcceptsBoolTrueFalseOneZero)
{
    baldr::HttpRequest request;
    request.query["name"]   = std::string("X");
    request.query["age"]    = std::string("1");
    request.query["active"] = std::string("1");
    request.query["weight"] = std::string("0.0");

    auto bound = baldr::detail::bindFromQuery<SearchFilters>(request);
    EXPECT_TRUE(bound.isOk());
    EXPECT_TRUE(bound.value.active);

    request.query["active"] = std::string("0");
    auto bound2 = baldr::detail::bindFromQuery<SearchFilters>(request);
    EXPECT_TRUE(bound2.isOk());
    EXPECT_FALSE(bound2.value.active);
}

TEST(FromQueryTest, RejectsUnsupportedBoolValue)
{
    baldr::HttpRequest request;
    request.query["name"]   = std::string("X");
    request.query["age"]    = std::string("1");
    request.query["active"] = std::string("yes");
    request.query["weight"] = std::string("0.0");

    auto bound = baldr::detail::bindFromQuery<SearchFilters>(request);
    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_NE(bound.error->message.find("could not be parsed"),
              std::string::npos);
}

TEST(FromQueryTest, StringViewFieldSharesLifetimeWithRequest)
{
    baldr::HttpRequest request;
    request.query["s"]   = std::string("hello");
    request.query["sv"]  = std::string("world");
    request.query["i"]   = std::string("1");
    request.query["i64"] = std::string("42");
    request.query["d"]   = std::string("3.14");
    request.query["f"]   = std::string("2.5");
    request.query["b"]   = std::string("false");

    auto bound = baldr::detail::bindFromQuery<AllSupported>(request);

    EXPECT_TRUE(bound.isOk());
    EXPECT_EQ(bound.value.s, "hello");
    EXPECT_EQ(bound.value.sv, "world");
    EXPECT_EQ(bound.value.i, 1);
    EXPECT_EQ(bound.value.i64, 42);
    EXPECT_DOUBLE_EQ(bound.value.d, 3.14);
    EXPECT_FLOAT_EQ(bound.value.f, 2.5f);
    EXPECT_FALSE(bound.value.b);
}

TEST(FromQueryTest, UnsupportedFieldTypeIsRejected)
{
    baldr::HttpRequest request;
    request.query["id"]  = std::string("1");
    request.query["raw"] = std::string("anything");

    EXPECT_FALSE((baldr::IsReflectableStruct<Unsupported>) );
}

TEST(FromQueryTest, IsFromQueryTraitDetectsWrapper)
{
    static_assert(baldr::isFromQuery_v<baldr::FromQuery<SearchFilters>>,
                  "isFromQuery_v must recognise FromQuery<U>");
    static_assert(!baldr::isFromQuery_v<SearchFilters>,
                  "isFromQuery_v must reject non-wrapper types");
    static_assert(!baldr::isFromQuery_v<int>,
                  "isFromQuery_v must reject primitives");
    static_assert(
        std::is_same_v<
            baldr::isFromQuery<baldr::FromQuery<SearchFilters>>::ValueType,
            SearchFilters>,
        "ValueType alias must surface the wrapped payload type");
}

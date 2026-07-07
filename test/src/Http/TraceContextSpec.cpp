#include <Baldr/Http/TraceContext.hpp>

#include <gtest/gtest.h>

class TraceContextSpec : public ::testing::Test
{
};

TEST_F(TraceContextSpec, ParsesValidV0Sampled)
{
    baldr::TraceContext tc;
    ASSERT_TRUE(baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01", tc));
    EXPECT_TRUE(tc.valid);
    EXPECT_EQ(tc.version, 0);
    EXPECT_EQ(tc.traceId, "0af7651916cd43dd8448eb211c80319c");
    EXPECT_EQ(tc.spanId, "b7ad6b7169203331");
    EXPECT_EQ(tc.traceFlags, 0x01);
    EXPECT_TRUE(tc.sampled());
}

TEST_F(TraceContextSpec, ParsesValidV0Unsampled)
{
    baldr::TraceContext tc;
    ASSERT_TRUE(baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-00", tc));
    EXPECT_EQ(tc.traceFlags, 0x00);
    EXPECT_FALSE(tc.sampled());
}

TEST_F(TraceContextSpec, EmptyHeaderIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent("", tc));
    EXPECT_FALSE(tc.valid);
}

TEST_F(TraceContextSpec, WhitespacePaddedHeaderIsTolerated)
{
    baldr::TraceContext tc;
    ASSERT_TRUE(baldr::TryParseTraceparent(
        "  00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01  ", tc));
    EXPECT_TRUE(tc.valid);
}

TEST_F(TraceContextSpec, WrongFieldCountIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331", tc));
    EXPECT_FALSE(baldr::TryParseTraceparent("00-only-three", tc));
}

TEST_F(TraceContextSpec, NonHexTraceIdIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent(
        "00-ZZZ7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01", tc));
}

TEST_F(TraceContextSpec, ShortTraceIdIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319-b7ad6b7169203331-01", tc));
}

TEST_F(TraceContextSpec, ShortSpanIdIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b716920333-01", tc));
}

TEST_F(TraceContextSpec, AllZeroTraceIdIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent(
        "00-00000000000000000000000000000000-b7ad6b7169203331-01", tc));
}

TEST_F(TraceContextSpec, AllZeroSpanIdIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-0000000000000000-01", tc));
}

TEST_F(TraceContextSpec, FutureVersionIsAcceptedAndParsed)
{
    baldr::TraceContext tc;
    ASSERT_TRUE(baldr::TryParseTraceparent(
        "01-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01", tc));
    EXPECT_TRUE(tc.valid);
    EXPECT_EQ(tc.version, 0x01);
    EXPECT_EQ(tc.traceId, "0af7651916cd43dd8448eb211c80319c");
    EXPECT_EQ(tc.spanId, "b7ad6b7169203331");
}

TEST_F(TraceContextSpec, InvalidFlagsIsInvalid)
{
    baldr::TraceContext tc;
    EXPECT_FALSE(baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-ZZ", tc));
}

TEST_F(TraceContextSpec, NewTraceIdIsNonZeroAndThirtyTwoChars)
{
    const auto id = baldr::NewTraceId();
    EXPECT_EQ(id.size(), 32u);
    EXPECT_FALSE(baldr::IsAllZeroHex(id));
    EXPECT_TRUE(baldr::IsLowerHex(id));
}

TEST_F(TraceContextSpec, NewSpanIdIsNonZeroAndSixteenChars)
{
    const auto id = baldr::NewSpanId();
    EXPECT_EQ(id.size(), 16u);
    EXPECT_FALSE(baldr::IsAllZeroHex(id));
    EXPECT_TRUE(baldr::IsLowerHex(id));
}

TEST_F(TraceContextSpec, NewTraceIdsAreDistinct)
{
    EXPECT_NE(baldr::NewTraceId(), baldr::NewTraceId());
}

TEST_F(TraceContextSpec, NewSpanIdsAreDistinct)
{
    EXPECT_NE(baldr::NewSpanId(), baldr::NewSpanId());
}
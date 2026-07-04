#include <Baldr/Http/TraceContext.hpp>

#include <gtest/gtest.h>

class TraceContextSpec : public ::testing::Test
{
};

TEST_F(TraceContextSpec, ParsesValidV0Sampled)
{
    Baldr::TraceContext tc;
    ASSERT_TRUE(Baldr::TryParseTraceparent(
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
    Baldr::TraceContext tc;
    ASSERT_TRUE(Baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-00", tc));
    EXPECT_EQ(tc.traceFlags, 0x00);
    EXPECT_FALSE(tc.sampled());
}

TEST_F(TraceContextSpec, EmptyHeaderIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent("", tc));
    EXPECT_FALSE(tc.valid);
}

TEST_F(TraceContextSpec, WhitespacePaddedHeaderIsTolerated)
{
    Baldr::TraceContext tc;
    ASSERT_TRUE(Baldr::TryParseTraceparent(
        "  00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01  ", tc));
    EXPECT_TRUE(tc.valid);
}

TEST_F(TraceContextSpec, WrongFieldCountIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331", tc));
    EXPECT_FALSE(Baldr::TryParseTraceparent("00-only-three", tc));
}

TEST_F(TraceContextSpec, NonHexTraceIdIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent(
        "00-ZZZ7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01", tc));
}

TEST_F(TraceContextSpec, ShortTraceIdIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319-b7ad6b7169203331-01", tc));
}

TEST_F(TraceContextSpec, ShortSpanIdIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b716920333-01", tc));
}

TEST_F(TraceContextSpec, AllZeroTraceIdIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent(
        "00-00000000000000000000000000000000-b7ad6b7169203331-01", tc));
}

TEST_F(TraceContextSpec, AllZeroSpanIdIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-0000000000000000-01", tc));
}

TEST_F(TraceContextSpec, FutureVersionIsAcceptedAndParsed)
{
    Baldr::TraceContext tc;
    ASSERT_TRUE(Baldr::TryParseTraceparent(
        "01-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01", tc));
    EXPECT_TRUE(tc.valid);
    EXPECT_EQ(tc.version, 0x01);
    EXPECT_EQ(tc.traceId, "0af7651916cd43dd8448eb211c80319c");
    EXPECT_EQ(tc.spanId, "b7ad6b7169203331");
}

TEST_F(TraceContextSpec, InvalidFlagsIsInvalid)
{
    Baldr::TraceContext tc;
    EXPECT_FALSE(Baldr::TryParseTraceparent(
        "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-ZZ", tc));
}

TEST_F(TraceContextSpec, NewTraceIdIsNonZeroAndThirtyTwoChars)
{
    const auto id = Baldr::NewTraceId();
    EXPECT_EQ(id.size(), 32u);
    EXPECT_FALSE(Baldr::IsAllZeroHex(id));
    EXPECT_TRUE(Baldr::IsLowerHex(id));
}

TEST_F(TraceContextSpec, NewSpanIdIsNonZeroAndSixteenChars)
{
    const auto id = Baldr::NewSpanId();
    EXPECT_EQ(id.size(), 16u);
    EXPECT_FALSE(Baldr::IsAllZeroHex(id));
    EXPECT_TRUE(Baldr::IsLowerHex(id));
}

TEST_F(TraceContextSpec, NewTraceIdsAreDistinct)
{
    EXPECT_NE(Baldr::NewTraceId(), Baldr::NewTraceId());
}

TEST_F(TraceContextSpec, NewSpanIdsAreDistinct)
{
    EXPECT_NE(Baldr::NewSpanId(), Baldr::NewSpanId());
}
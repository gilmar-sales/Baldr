#include <Baldr/Http/Results/StreamingResult.hpp>

#include <string>
#include <vector>

TEST(StreamingResult, FormatChunkWrapsDataInChunkedEnvelope)
{
    std::string data  = "hello";
    std::string frame = baldr::formatChunk(data);

    EXPECT_EQ(frame, "5\r\nhello\r\n");
}

TEST(StreamingResult, FormatChunkEmptyDataProducesZeroLengthChunk)
{
    std::string frame = baldr::formatChunk("");
    EXPECT_EQ(frame, "0\r\n\r\n");
}

TEST(StreamingResult, FormatChunkTrailerIsZeroLengthTerminator)
{
    EXPECT_EQ(baldr::formatChunkTrailer(), "0\r\n\r\n");
}

TEST(StreamingResult, FormatStreamingHeadAddsTransferEncodingChunked)
{
    std::vector<std::pair<std::string, std::string>> headers = {
        { "Content-Type", "text/event-stream" }
    };
    std::vector<std::pair<std::string, std::string>> cookies;
    auto head = baldr::formatStreamingHead(
        baldr::StatusCode::OK, "HTTP/1.1", headers, cookies,
        [](baldr::StatusCode s) -> const char* {
            switch (s)
            {
                case baldr::StatusCode::OK:
                    return "OK";
                default:
                    return "OK";
            }
        });

    EXPECT_NE(head.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_NE(head.find("Content-Type: text/event-stream\r\n"),
              std::string::npos);
    EXPECT_NE(head.find("Transfer-Encoding: chunked\r\n"), std::string::npos);
    EXPECT_NE(head.find("\r\n\r\n"), std::string::npos);
}

TEST(StreamingResult, ChunkedStreamResultEmitsChunksUntilProducerReturnsFalse)
{
    std::vector<std::string>   emitted;
    int                        counter = 0;
    baldr::ChunkedStreamResult result([&](std::string& out) -> bool {
        if (counter >= 3)
            return false;
        out = "chunk-" + std::to_string(counter++);
        return true;
    });

    std::string chunk;
    while (result.nextChunk(chunk))
        emitted.push_back(chunk);

    ASSERT_EQ(emitted.size(), 3u);
    EXPECT_EQ(emitted[0], "chunk-0");
    EXPECT_EQ(emitted[1], "chunk-1");
    EXPECT_EQ(emitted[2], "chunk-2");
}

TEST(StreamingResult, ChunkedStreamResultWithNoProducerEmitsNoChunks)
{
    baldr::ChunkedStreamResult result(nullptr);
    std::string                chunk;
    EXPECT_FALSE(result.nextChunk(chunk));
}

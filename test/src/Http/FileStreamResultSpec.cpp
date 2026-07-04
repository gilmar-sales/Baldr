#include <Baldr/Http/Results/FileStreamResult.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

fs::path makeTempFile(const std::string& contents, const std::string& tag)
{
    auto dir = fs::temp_directory_path() / ("baldr-filestreamresult-" + tag);
    fs::create_directories(dir);
    auto          path = dir / "data.bin";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return path;
}

TEST(FileStreamResult, HeadersIncludesContentTypeAndDisposition)
{
    std::ifstream           empty;
    baldr::FileStreamResult r(std::move(empty), "application/pdf", "baldr.pdf");

    std::vector<std::pair<std::string, std::string>> headers;
    r.headers(headers);

    ASSERT_EQ(headers.size(), 2u);
    EXPECT_EQ(headers[0].first, "Content-Type");
    EXPECT_EQ(headers[0].second, "application/pdf");
    EXPECT_EQ(headers[1].first, "Content-Disposition");
    EXPECT_EQ(headers[1].second, "attachment; filename=\"baldr.pdf\"");
}

TEST(FileStreamResult, NextChunkEmitsFileBytes)
{
    const std::string payload = "hello world\n";
    auto              path    = makeTempFile(payload, "emit");

    std::ifstream           in(path, std::ios::binary);
    baldr::FileStreamResult r(std::move(in), "text/plain", "data.bin");

    std::string concatenated;
    std::string chunk;
    while (r.nextChunk(chunk))
        concatenated.append(chunk);

    EXPECT_EQ(concatenated, payload);
}

TEST(FileStreamResult, NextChunkReturnsFalseAfterEof)
{
    const std::string payload = "tiny";
    auto              path    = makeTempFile(payload, "eof");

    std::ifstream           in(path, std::ios::binary);
    baldr::FileStreamResult r(std::move(in), "text/plain", "data.bin");

    std::string chunk;
    while (r.nextChunk(chunk))
    {
    }

    std::string after;
    EXPECT_FALSE(r.nextChunk(after));
    EXPECT_TRUE(after.empty());
}

TEST(FileStreamResult, NextChunkReturnsFalseWhenFileCannotBeOpened)
{
    std::ifstream           empty;
    baldr::FileStreamResult r(std::move(empty),
                              "application/octet-stream",
                              "missing.bin");

    std::string chunk;
    EXPECT_FALSE(r.nextChunk(chunk));
    EXPECT_TRUE(chunk.empty());
}

TEST(FileStreamResult, EmitsMultiChunkWhenPayloadExceedsBuffer)
{
    std::string payload;
    payload.reserve(baldr::FileStreamResult::kDefaultChunkBytes * 2 + 17);
    for (std::size_t i = 0;
         i < baldr::FileStreamResult::kDefaultChunkBytes * 2 + 17;
         ++i)
    {
        payload.push_back(static_cast<char>('a' + (i % 26)));
    }
    auto path = makeTempFile(payload, "multi");

    std::ifstream           in(path, std::ios::binary);
    baldr::FileStreamResult r(std::move(in),
                              "application/octet-stream",
                              "data.bin");

    std::size_t total = 0;
    int         calls = 0;
    std::string chunk;
    while (r.nextChunk(chunk))
    {
        total += chunk.size();
        ++calls;
    }

    EXPECT_EQ(total, payload.size());
    EXPECT_GE(calls, 3);
}

TEST(FileStreamResult, StreamIsReleasedAfterDestruction)
{
    const std::string payload = "release";
    auto              path    = makeTempFile(payload, "release");

    {
        std::ifstream           in(path, std::ios::binary);
        baldr::FileStreamResult r(std::move(in), "text/plain", "data.bin");
    }

    std::error_code ec;
    fs::remove(path, ec);
    EXPECT_FALSE(ec);
}
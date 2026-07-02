#include "Baldr/StaticFilesInternal.hpp"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace
{
    class MapStaticFilesSpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mRoot = std::filesystem::temp_directory_path() /
                    ("baldr_static_" +
                     std::to_string(::testing::UnitTest::GetInstance()
                                        ->random_seed()));
            std::filesystem::remove_all(mRoot);
            std::filesystem::create_directories(mRoot / "css");
            std::filesystem::create_directories(mRoot / "assets");
            std::filesystem::create_directories(mRoot / "assets" / "img");

            write(mRoot / "index.html", "<!doctype html>root");
            write(mRoot / "css" / "site.css", "body{}");
            write(mRoot / "assets" / "app.js", "console.log(1)");
            write(mRoot / "assets" / "hello.txt", "hi");
            write(mRoot / "assets" / "img" / "logo.svg", "<svg/>");
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all(mRoot, ec);
        }

        static void write(const std::filesystem::path& p,
                          const std::string&            content)
        {
            std::ofstream(p) << content;
        }

        std::filesystem::path mRoot;
    };

    using Baldr::Detail::resolveStaticFile;
}

TEST_F(MapStaticFilesSpec, ServesRootIndexHtml)
{
    auto r = resolveStaticFile("index.html", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::OK);
    EXPECT_EQ(r.body, "<!doctype html>root");
    EXPECT_EQ(r.mimeType, "text/html");
}

TEST_F(MapStaticFilesSpec, ServesNestedCss)
{
    auto r = resolveStaticFile("css/site.css", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::OK);
    EXPECT_EQ(r.body, "body{}");
    EXPECT_EQ(r.mimeType, "text/css");
}

TEST_F(MapStaticFilesSpec, ServesDeeplyNestedSvg)
{
    auto r = resolveStaticFile("assets/img/logo.svg", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::OK);
    EXPECT_EQ(r.body, "<svg/>");
    EXPECT_EQ(r.mimeType, "image/svg+xml");
}

TEST_F(MapStaticFilesSpec, ServesTxtAsPlain)
{
    auto r = resolveStaticFile("assets/hello.txt", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::OK);
    EXPECT_EQ(r.mimeType, "text/plain");
}

TEST_F(MapStaticFilesSpec, ServesDirectoryIndexWhenEmptyRemainder)
{
    auto r = resolveStaticFile("", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::OK);
    EXPECT_EQ(r.body, "<!doctype html>root");
}

TEST_F(MapStaticFilesSpec, DirectoryWithoutIndexReturns404)
{
    std::filesystem::create_directories(mRoot / "empty");
    auto r = resolveStaticFile("empty", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::NotFound);
}

TEST_F(MapStaticFilesSpec, MissingFileReturns404)
{
    auto r = resolveStaticFile("does/not/exist.txt", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::NotFound);
}

TEST_F(MapStaticFilesSpec, RejectsDotDotTraversal)
{
    auto r = resolveStaticFile("../etc/passwd", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::BadRequest);
}

TEST_F(MapStaticFilesSpec, RejectsEncodedTraversalAfterDecoding)
{
    // HttpRequestParser percent-decodes the path before the static-files
    // handler runs, so by the time resolveStaticFile sees these strings
    // they are already decoded into the canonical traversal form.
    auto r = resolveStaticFile("../etc/passwd", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::BadRequest);

    auto r2 = resolveStaticFile("foo/../etc/passwd", mRoot.string());
    EXPECT_EQ(r2.status, StatusCode::BadRequest);
}

TEST_F(MapStaticFilesSpec, RejectsBackslashAsPathSeparator)
{
    auto r = resolveStaticFile("foo\\..\\bar", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::BadRequest);
}

TEST_F(MapStaticFilesSpec, RejectsNulByte)
{
    auto r = resolveStaticFile(std::string("good\0bad", 8), mRoot.string());
    EXPECT_EQ(r.status, StatusCode::BadRequest);
}

TEST_F(MapStaticFilesSpec, RejectsDotSegment)
{
    auto r = resolveStaticFile("./index.html", mRoot.string());
    EXPECT_EQ(r.status, StatusCode::BadRequest);
}
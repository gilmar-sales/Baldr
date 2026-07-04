#include <Baldr/Baldr.hpp>
#include <Baldr/Http/Results/FileStreamResult.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

fs::path assetsDir()
{
    return fs::path(__FILE__).parent_path().parent_path() / "assets";
}

fs::path uploadsDir()
{
    return fs::path(__FILE__).parent_path().parent_path() / "uploads";
}

std::string fnv1a64(std::string_view data)
{
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char c : data)
    {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 0x100000001b3ULL;
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

struct UploadResponse
{
    std::string storedAs;
    std::size_t bytes;
    std::string digest;
    std::string contentType;
};

std::int64_t nowMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    auto app = builder.Build<baldr::WebApplication>();

    const auto      assets  = assetsDir();
    const auto      pdfPath = assets / "baldr.pdf";
    const auto      uploads = uploadsDir();
    std::error_code ec;
    fs::create_directories(uploads, ec);

    app->MapGet("/files/baldr.pdf", [pdfPath] {
        std::ifstream in(pdfPath, std::ios::binary);
        return baldr::FileStreamResult(std::move(in),
                                       "application/pdf",
                                       "baldr.pdf");
    });

    app->MapPost(
        "/upload",
        [uploads](const baldr::HttpRequest& request) -> baldr::JsonResult {
            const auto storedAs =
                "upload-" + std::to_string(nowMillis()) + ".bin";
            const auto outPath = uploads / storedAs;

            std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                return baldr::JsonResult(
                    std::string("{\"error\":\"could not open output file\"}"),
                    baldr::StatusCode::InternalServerError);
            }

            if (!request.body.empty())
            {
                out.write(request.body.data(),
                          static_cast<std::streamsize>(request.body.size()));
            }
            out.flush();

            const auto digest = fnv1a64(request.body);

            std::string contentType = "application/octet-stream";
            if (auto it = request.headers.find("Content-Type");
                it != request.headers.end())
            {
                contentType = it->second;
            }

            return baldr::Results::Json(UploadResponse {
                storedAs, request.body.size(), digest, contentType });
        });

    app->Run();

    return 0;
}
# File stream example

[`examples/FileStream`](https://github.com/gilmar-sales/Baldr/tree/main/examples/FileStream) combines a streaming file download (`baldr::FileStreamResult`) with a JSON upload handler that persists the body to disk and returns metadata.

## Source

[`examples/FileStream/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/FileStream/src/main.cpp):

```cpp title="examples/FileStream/src/main.cpp" linenums="1"
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
#include <variant>

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
        [uploads](const baldr::HttpRequest& request)
            -> std::variant<
                baldr::JsonResult<UploadResponse, baldr::StatusCode::OK>,
                baldr::InternalServerErrorResult> {
            const auto storedAs =
                "upload-" + std::to_string(nowMillis()) + ".bin";
            const auto outPath = uploads / storedAs;

            std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                return baldr::Results::InternalServerError(
                    "could not open output file");
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

            return baldr::Results::Json<UploadResponse, baldr::StatusCode::OK>(
                UploadResponse { storedAs, request.body.size(), digest,
                                 contentType });
        });

    app->Run();

    return 0;
}
```

## What it shows

- Streaming a file from disk with [`baldr::FileStreamResult`](../../usage/streaming-results.md#filestreamresult). The response sets `Content-Disposition: attachment; filename="baldr.pdf"`.
- Reading the raw request body in a handler — no JSON parsing, `request.body` is a `std::string` and `request.headers` is a header map.
- Returning `baldr::Results::Json<T, StatusCode>(value)` from a `MapPost` handler to serialise a typed response struct with a specific status code.
- Returning a typed error result (`baldr::InternalServerErrorResult`) from the same `std::variant` when the upload cannot be persisted.
- Resolving example assets relative to `__FILE__` so the program works regardless of the current working directory.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/FileStream
```

In another terminal:

```bash
curl -OJ http://localhost:8080/files/baldr.pdf

curl -X POST --data-binary @README.md \
     -H 'Content-Type: text/plain' \
     http://localhost:8080/upload
# {"storedAs":"upload-1700000000000.bin","bytes":...,"digest":"...","contentType":"text/plain"}
```

## Next steps

- See [Streaming results](../../usage/streaming-results.md) for the underlying `IStreamingResult` mechanism.
- See [Results](../../usage/results.md) for the full `IResult` family and `Results::Json`.
- Browse [all examples](../examples.md).

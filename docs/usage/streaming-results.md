# Streaming results

Most Baldr handlers return synchronously — a value, an `IResult` subclass, or `void`. When the body is large or produced lazily, return an [`IStreamingResult`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/Http/Results/StreamingResult.hpp) instead. The framework will emit `Transfer-Encoding: chunked` and stream chunks as the producer makes them available.

## When to use streaming

- The body is large (multi-megabyte file, generated report) and you want to avoid buffering it all in memory.
- The body is produced incrementally — server-sent events, tailing a log, proxying a producer-consumer pipeline.
- You need backpressure-friendly reads where the producer can stop producing if the client disconnects.

## `IStreamingResult`

```cpp title="StreamingResult.hpp" linenums="1"
class IStreamingResult
{
  public:
    virtual ~IStreamingResult() = default;

    // Optional response headers. Implementations may set
    // "Content-Type" here. Implementations MUST NOT set
    // "Content-Length" or "Transfer-Encoding"; the framework sets
    // Transfer-Encoding: chunked automatically.
    virtual void headers(std::vector<std::pair<std::string, std::string>>& out) const
    {
        (void)out;
    }

    virtual StatusCode statusCode() const { return StatusCode::OK; }

    virtual bool nextChunk(std::string& out) const = 0;
};
```

Two rules:

1. Implementations **must not** set `Content-Length` or `Transfer-Encoding` headers — the framework writes `Transfer-Encoding: chunked` itself.
2. `nextChunk` is called repeatedly; returning `false` signals end-of-body. Mutable state is allowed on the implementation because `nextChunk` is `const` only by convention (the result object is held inside a `std::shared_ptr` so it can outlive the handler call).

## Built-in streaming results

### `ChunkedStreamResult`

Driven by a user-supplied callback that fills the next chunk. Useful for SSE or for reading from a producer.

```cpp title="src/main.cpp"
app->MapGet("/events", [] {
    return ChunkedStreamResult([](std::string& out) -> bool {
        out = "data: tick\n\n";
        return true;
    });
});
```

`ChunkedStreamResult::Producer` is `std::function<bool(std::string&)>`. Returning `false` terminates the stream.

### `FileStreamResult`

Streams a file from disk in 64 KiB chunks. The response sets `Content-Type` and `Content-Disposition: attachment; filename="..."`.

```cpp title="src/main.cpp"
#include <Baldr/Http/Results/FileStreamResult.hpp>
#include <fstream>

app->MapGet("/report.pdf", [] {
    std::ifstream in("/var/data/report.pdf", std::ios::binary);
    return FileStreamResult(std::move(in), "application/pdf", "report.pdf");
});
```

The full program is in [`examples/FileStream`](https://github.com/gilmar-sales/Baldr/tree/main/examples/FileStream).

## Custom streaming results

Derive from `IStreamingResult` and implement `nextChunk`. Use `headers()` for `Content-Type` and any other fixed headers (for example `Cache-Control`).

```cpp title="SseStreamResult.hpp"
#pragma once

#include <Baldr/Http/Results/StreamingResult.hpp>

class SseStreamResult final : public IStreamingResult
{
  public:
    void headers(std::vector<std::pair<std::string, std::string>>& out) const override
    {
        out.clear();
        out.emplace_back("Content-Type", "text/event-stream");
        out.emplace_back("Cache-Control", "no-cache");
    }

    bool nextChunk(std::string& out) const override
    {
        if (finished)
            return false;
        out = "data: " + nextEvent() + "\n\n";
        return true;
    }
};
```

## Interaction with middleware

`CompressionMiddleware` skips streaming responses — chunked transfer encoding is incompatible with on-the-fly body rewrites, so streamed payloads are sent uncompressed regardless of `Accept-Encoding`. Use a buffered `IResult` for compressible responses.

`Response.streaming` is set by the framework after the handler runs (`src/Baldr/Http/Router.hpp` and `src/Baldr/Application/WebApplication.hpp`) — middleware that needs to inspect or replace streaming bodies can read this field before passing control on.

## Next steps

- See the [`examples/FileStream`](https://github.com/gilmar-sales/Baldr/tree/main/examples/FileStream) program for a complete file-streaming endpoint and an upload handler.
- Combine with [Static files](static-files.md) for efficient static-asset serving.
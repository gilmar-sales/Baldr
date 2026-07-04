/**
 * @file Http/Results/FileStreamResult.hpp
 * @brief Streaming result that reads from an open @c std::ifstream in
 *        64 KiB chunks and forces a download via @c Content-Disposition.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <array>
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <Baldr/Http/Results/StreamingResult.hpp>

namespace BALDR_NAMESPACE {

/**
 * @brief Streams a file to the client via chunked transfer encoding.
 *
 * The file is read lazily in @ref kDefaultChunkBytes chunks; EOF
 * terminates the stream. The response advertises @c Content-Type and a
 * @c Content-Disposition: attachment header carrying the original
 * filename.
 */
class FileStreamResult final : public IStreamingResult
{
  public:
    /// Size of each read chunk sent to the client.
    static constexpr std::size_t kDefaultChunkBytes = 64 * 1024;

    /**
     * @brief Wrap an already-opened file in a streaming result.
     *
     * @param file        Open input stream positioned at the start of the
     *                    payload (taken by move; the stream is closed
     *                    automatically when the result is destroyed).
     * @param contentType MIME type to advertise.
     * @param fileName    File name used in the @c Content-Disposition
     *                    header.
     */
    FileStreamResult(std::ifstream file,
                     std::string   contentType,
                     std::string   fileName) :
        mFile(std::move(file)), mContentType(std::move(contentType)),
        mFileName(std::move(fileName))
    {
    }

    void headers(
        std::vector<std::pair<std::string, std::string>>& out) const override
    {
        out.clear();
        out.emplace_back("Content-Type", mContentType);
        out.emplace_back("Content-Disposition",
                         "attachment; filename=\"" + mFileName + "\"");
    }

    bool nextChunk(std::string& out) const override
    {
        if (!mFile || mFile.eof())
            return false;

        std::array<char, kDefaultChunkBytes> buffer {};
        mFile.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        auto got = mFile.gcount();
        if (got <= 0)
            return false;

        out.assign(buffer.data(), static_cast<std::size_t>(got));
        return true;
    }

  private:
    mutable std::ifstream mFile;
    std::string           mContentType;
    std::string           mFileName;
};

} // namespace BALDR_NAMESPACE
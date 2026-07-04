#pragma once

#include <array>
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <Baldr/Http/Results/StreamingResult.hpp>

class FileStreamResult final : public IStreamingResult
{
  public:
    static constexpr std::size_t kDefaultChunkBytes = 64 * 1024;

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
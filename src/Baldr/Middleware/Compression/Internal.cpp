#include <Baldr/Detail/Namespace.hpp>
#include <Baldr/Middleware/Compression/Internal.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

#include <zlib.h>

namespace BALDR_NAMESPACE
{

    namespace Detail
    {
        constexpr std::size_t kChunkSize = 16 * 1024;

        bool gzipCompress(std::string_view input, std::string& output,
                          int level)
        {
            output.clear();
            if (input.empty())
                return true;

            z_stream stream {};
            // windowBits = 15 + 16 selects gzip framing per zlib docs.
            if (deflateInit2(&stream, level, Z_DEFLATED, 15 + 16, 8,
                             Z_DEFAULT_STRATEGY) != Z_OK)
            {
                return false;
            }

            stream.next_in =
                reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
            stream.avail_in = static_cast<uInt>(input.size());

            std::string buf;
            buf.reserve(input.size() / 2 + 32);

            int ret = Z_OK;
            do
            {
                char out[kChunkSize];
                stream.next_out  = reinterpret_cast<Bytef*>(out);
                stream.avail_out = static_cast<uInt>(sizeof(out));

                ret = deflate(&stream, Z_FINISH);
                if (ret != Z_OK && ret != Z_STREAM_END)
                {
                    deflateEnd(&stream);
                    return false;
                }

                std::size_t produced = sizeof(out) - stream.avail_out;
                buf.append(out, produced);
            } while (ret == Z_OK && stream.avail_out == 0);

            deflateEnd(&stream);
            if (ret != Z_STREAM_END)
                return false;

            output = std::move(buf);
            return true;
        }

        bool gzipDecompress(std::string_view input, std::string& output)
        {
            output.clear();
            if (input.empty())
                return true;

            z_stream stream {};
            if (inflateInit2(&stream, 15 + 16) != Z_OK)
                return false;

            stream.next_in =
                reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
            stream.avail_in = static_cast<uInt>(input.size());

            std::string buf;
            char        out[kChunkSize];
            int         ret = Z_OK;
            do
            {
                stream.next_out  = reinterpret_cast<Bytef*>(out);
                stream.avail_out = static_cast<uInt>(sizeof(out));
                ret              = inflate(&stream, Z_NO_FLUSH);
                if (ret != Z_OK && ret != Z_STREAM_END)
                {
                    inflateEnd(&stream);
                    return false;
                }
                std::size_t produced = sizeof(out) - stream.avail_out;
                buf.append(out, produced);
            } while (ret == Z_OK && stream.avail_out == 0);

            inflateEnd(&stream);
            if (ret != Z_STREAM_END)
                return false;
            output = std::move(buf);
            return true;
        }
    } // namespace Detail

} // namespace BALDR_NAMESPACE

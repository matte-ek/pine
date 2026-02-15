#include "File.hpp"
#include <filesystem>
#include <optional>
#include <fstream>
#include <zlib.h>

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Performance/Performance.hpp"

Pine::ByteSpan Pine::File::ReadRaw(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);

    // Get size of file
    file.seekg(0, std::ios::end);
    const size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto data = new char[size];

    file.read(data, size);
    file.close();

    return { reinterpret_cast<std::byte*>(data), size };
}

void Pine::File::WriteRaw(const std::filesystem::path& path, const ByteSpan& byteSpan)
{
    std::ofstream file(path, std::ios::out | std::ios::binary);

    file.write(reinterpret_cast<char*>(byteSpan.data), byteSpan.size);
    file.close();
}

Pine::ByteSpan Pine::File::ReadCompressed(const std::filesystem::path& path)
{
    constexpr size_t CHUNK_SIZE = 16384;

    const auto data = ReadRaw(path);

    z_stream strm{};

    strm.next_in = reinterpret_cast<Bytef*>(data.data);
    strm.avail_in = data.size;

    auto ret = inflateInit(&strm);
    if (ret != Z_OK)
    {
        Pine::Log::Error(fmt::format("Failed to uncompress file to path {} with result {}", path.string(), ret));
        return {nullptr, 0};
    }

    std::vector<std::byte> output;
    std::byte buffer[CHUNK_SIZE];

    do
    {
        strm.next_out = reinterpret_cast<Bytef*>(buffer);
        strm.avail_out = CHUNK_SIZE;

        ret = inflate(&strm, Z_NO_FLUSH);

        if (ret == Z_STREAM_ERROR)
        {
            inflateEnd(&strm);

            Pine::Log::Error(fmt::format("Failed to uncompress file to path {} with result {}", path.string(), ret));
            return {nullptr, 0};
        }

        size_t bytesProduced = CHUNK_SIZE - strm.avail_out;

        output.insert(output.end(), buffer, buffer + bytesProduced);
    }
    while (ret != Z_STREAM_END);

    ByteSpan span(output.data(), output.size());

    return std::move(span);
}

void Pine::File::WriteCompressed(const std::filesystem::path& path, const ByteSpan& byteSpan)
{
    uLong compressedLength = compressBound(byteSpan.size);
    auto compressed = new Bytef[compressedLength];

    auto ret = compress(compressed, &compressedLength, reinterpret_cast<Bytef *>(byteSpan.data), byteSpan.size);
    if (ret != Z_OK)
    {
        Pine::Log::Error(fmt::format("Failed to compress file to path {} with result {}", path.string()));
        return;
    }

    WriteRaw(path, ByteSpan(reinterpret_cast<std::byte*>(compressed), compressedLength));

    delete[] compressed;
}

std::optional<std::string> Pine::File::ReadFile(std::filesystem::path path)
{
    if (!std::filesystem::exists(path))
    {
        return std::nullopt;
    }

    std::ifstream stream(path);

    if (!stream.is_open())
    {
        return std::nullopt;
    }

   return std::make_optional(std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>()));
}
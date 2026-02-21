#pragma once

#include <optional>
#include <string>
#include <filesystem>

#include "Pine/Core/Span/Span.hpp"

namespace Pine::File
{

    ByteSpan ReadRaw(const std::filesystem::path& path);
    void WriteRaw(const std::filesystem::path& path, const ByteSpan& byteSpan);

    ByteSpan ReadCompressed(const std::filesystem::path& path);
    void WriteCompressed(const std::filesystem::path& path, const ByteSpan& byteSpan);

    std::optional<std::string> ReadFile(std::filesystem::path path);

    std::string UniversalPath(const std::string& pathString);
    std::string UniversalPath(const std::filesystem::path& path);

}


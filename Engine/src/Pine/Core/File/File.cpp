#include "File.hpp"
#include <filesystem>
#include <optional>
#include <fstream>

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
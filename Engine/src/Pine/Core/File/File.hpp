#pragma once

#include <optional>
#include <string>
#include <filesystem>

namespace Pine::File
{

    std::optional<std::string> ReadFile(std::filesystem::path path);

}


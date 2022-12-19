#pragma once
#include <json.hpp>
#include <string>
#include <filesystem>
#include <optional>

namespace Pine::Serialization
{

    std::optional<nlohmann::json> LoadFromFile(const std::filesystem::path& path);

}
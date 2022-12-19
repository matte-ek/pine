#include "Serialization.hpp"
#include "Pine/Core/Log/Log.hpp"
#include <fstream>

std::optional<nlohmann::json> Pine::Serialization::LoadFromFile(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
        return {};

    std::ifstream fileStream(path);

    if (!fileStream.is_open())
        return {};

    nlohmann::json j;

    try
    {
        fileStream >> j;
    }
    catch (std::exception& e)
    {
        Log::Error("Error loading JSON file, " + path.filename().string());
        return {};
    }

    return j;
}

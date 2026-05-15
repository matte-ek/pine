#pragma once
#include <filesystem>

namespace Pine::Importer::ImageLoader
{
    void* LoadImage(const std::filesystem::path& file, int& width, int& height, int& channels);
}

#include "ImageLoader.hpp"

#include "Pine/Core/String/String.hpp"

namespace Pine::Importer::ImageLoader
{
    void* LoadImagePng(const std::filesystem::path& file, int& width, int& height, int& channels);
    void* LoadImageJpeg(const std::filesystem::path& file, int& width, int& height, int& channels);
}

void* Pine::Importer::ImageLoader::LoadImage(const std::filesystem::path& file, int& width, int& height, int& channels)
{
    const auto extension = file.extension().string();

    if (String::ToLower(extension) == ".png")
    {
        return LoadImagePng(file, width, height, channels);
    }

    if (String::ToLower(extension) == ".jpg" || String::ToLower(extension) == ".jpeg")
    {
        return LoadImageJpeg(file, width, height, channels);
    }

    return nullptr;
}

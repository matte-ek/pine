#include "AssetImportSettings.hpp"

#include "Gui/Shared/Widgets/Widgets.hpp"

Editor::Gui::AssetImportSettings::TextureChanges Editor::Gui::AssetImportSettings::RenderTexture(
    Pine::TextureImportConfiguration& configuration)
{
    TextureChanges changes;

    auto usageHint = static_cast<int>(configuration.UsageHint);
    auto compressionQuality = static_cast<int>(configuration.CompressionQuality);

    changes.CompressionQuality = Widgets::DropDown(
        "Compression Quality",
        &compressionQuality,
        "Normal\0Fastest\0Production\0");

    if (changes.CompressionQuality)
    {
        configuration.CompressionQuality = static_cast<Pine::TextureCompressionQuality>(compressionQuality);
    }

    changes.UsageHint = Widgets::DropDown(
        "Usage Hint",
        &usageHint,
        "Albedo (BC7)\0Albedo Fast (BC1)\0Normal (BC5)\0Grayscale (BC4)\0Data Map\0Raw\0Linear Color (BC7)\0");

    if (changes.UsageHint)
    {
        // Picked by hand, which outranks anything the importer can work out on its own.
        Pine::ApplyTextureUsageHint(
            configuration,
            static_cast<Pine::TextureUsageHint>(usageHint),
            Pine::TextureUsageHintSource::User);
    }

    changes.GenerateMipmaps = Widgets::Checkbox("Generate mip maps", &configuration.GenerateMipmaps);

    return changes;
}

void Editor::Gui::AssetImportSettings::ApplyTextureChanges(
    const Pine::TextureImportConfiguration& from,
    Pine::TextureImportConfiguration& to,
    const TextureChanges& changes)
{
    if (changes.UsageHint)
    {
        Pine::ApplyTextureUsageHint(to, from.UsageHint, from.UsageHintSource);
    }

    if (changes.CompressionQuality)
    {
        to.CompressionQuality = from.CompressionQuality;
    }

    if (changes.GenerateMipmaps)
    {
        to.GenerateMipmaps = from.GenerateMipmaps;
    }
}

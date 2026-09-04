#pragma once

#include "Pine/Assets/Texture2D/Texture2D.hpp"

// The settings an asset is imported with, as the user sees them. Drawn in two places - the
// properties panel, where they describe an import that already happened, and the import dialog,
// where they describe one about to happen - so they live here rather than in either of them.
namespace Editor::Gui::AssetImportSettings
{
    // Which settings the user changed this frame. A caller editing a whole selection at once needs
    // to know: it can then write just the field that changed to the rest of the selection, instead
    // of overwriting every field with the values it happened to be displaying.
    struct TextureChanges
    {
        bool UsageHint = false;
        bool CompressionQuality = false;
        bool GenerateMipmaps = false;

        bool Any() const
        {
            return UsageHint || CompressionQuality || GenerateMipmaps;
        }
    };

    // Draws the settings a texture is (or will be) imported with, editing 'configuration' in
    // place. Picking a usage hint by hand records that it was picked by hand, which is what stops
    // a later re-import from guessing over it.
    TextureChanges RenderTexture(Pine::TextureImportConfiguration& configuration);

    // Copies the fields marked in 'changes' from 'from' onto 'to', for applying one edit across a
    // multi-selection.
    void ApplyTextureChanges(
        const Pine::TextureImportConfiguration& from,
        Pine::TextureImportConfiguration& to,
        const TextureChanges& changes);
}

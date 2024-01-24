#include "AssetPropertiesRenderer.hpp"

#include "IconsMaterialDesign.h"
#include "imgui.h"

#include "Gui/Shared/IconStorage/IconStorage.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/Level/Level.hpp"

namespace
{

    void RenderTexture2D(const Pine::Texture2D *texture2d)
    {
        const auto textureAspectRatio = static_cast<float>(texture2d->GetWidth()) / static_cast<float>(texture2d->GetHeight());
        const auto previewWidth = ImGui::GetContentRegionAvail().x * 0.5f;
        const auto previewHeight = previewWidth / textureAspectRatio;

        ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t *>(texture2d->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(previewWidth, previewHeight));

        ImGui::Text("Width: %d", texture2d->GetWidth());
        ImGui::Text("Height: %d", texture2d->GetHeight());
        ImGui::Text("Format: %s", Pine::Graphics::TextureFormatToString(texture2d->GetFormat()));
        ImGui::Text("Has mip-maps: %s", texture2d->GetGenerateMipmaps() ? "true" : "false");
    }

    void RenderTexture3D(Pine::Texture3D *texture3d)
    {
        ImGui::Text("Valid: %s", texture3d->IsValid() ? "Yes" : "No");

        const auto front = texture3d->GetSideTexture(Pine::TextureCubeSide::Front);
        const auto frontResult = Widgets::AssetPicker("Front", front, Pine::AssetType::Texture2D);

        const auto back = texture3d->GetSideTexture(Pine::TextureCubeSide::Back);
        const auto backResult = Widgets::AssetPicker("Back", back, Pine::AssetType::Texture2D);

        const auto left = texture3d->GetSideTexture(Pine::TextureCubeSide::Left);
        const auto leftResult = Widgets::AssetPicker("Left", left, Pine::AssetType::Texture2D);

        const auto right = texture3d->GetSideTexture(Pine::TextureCubeSide::Right);
        const auto rightResult = Widgets::AssetPicker("Right", right, Pine::AssetType::Texture2D);

        const auto top = texture3d->GetSideTexture(Pine::TextureCubeSide::Top);
        const auto topResult = Widgets::AssetPicker("Top", top, Pine::AssetType::Texture2D);

        const auto bottom = texture3d->GetSideTexture(Pine::TextureCubeSide::Bottom);
        const auto bottomResult = Widgets::AssetPicker("Bottom", bottom, Pine::AssetType::Texture2D);

        if (frontResult.hasResult)
            texture3d->SetSideTexture(Pine::TextureCubeSide::Front, dynamic_cast<Pine::Texture2D *>(frontResult.asset));
        if (backResult.hasResult)
            texture3d->SetSideTexture(Pine::TextureCubeSide::Back, dynamic_cast<Pine::Texture2D *>(backResult.asset));
        if (leftResult.hasResult)
            texture3d->SetSideTexture(Pine::TextureCubeSide::Left, dynamic_cast<Pine::Texture2D *>(leftResult.asset));
        if (rightResult.hasResult)
            texture3d->SetSideTexture(Pine::TextureCubeSide::Right, dynamic_cast<Pine::Texture2D *>(rightResult.asset));
        if (topResult.hasResult)
            texture3d->SetSideTexture(Pine::TextureCubeSide::Top, dynamic_cast<Pine::Texture2D *>(topResult.asset));
        if (bottomResult.hasResult)
            texture3d->SetSideTexture(Pine::TextureCubeSide::Bottom, dynamic_cast<Pine::Texture2D *>(bottomResult.asset));

        if (ImGui::Button("Build"))
        {
            texture3d->Build();
        }
    }

    void RenderShader(Pine::Shader *shader)
    {
    }

    void RenderMaterial(Pine::Material *material)
    {
        const auto diffuseResult = Widgets::AssetPicker("Diffuse Map", material->GetDiffuse(), Pine::AssetType::Texture2D);
        const auto specularResult = Widgets::AssetPicker("Specular Map", material->GetSpecular(), Pine::AssetType::Texture2D);
        const auto normalResult = Widgets::AssetPicker("Normal Map", material->GetNormal(), Pine::AssetType::Texture2D);

        if (diffuseResult.hasResult)
            material->SetDiffuse(dynamic_cast<Pine::Texture2D *>(diffuseResult.asset));
        if (specularResult.hasResult)
            material->SetSpecular(dynamic_cast<Pine::Texture2D *>(specularResult.asset));
        if (normalResult.hasResult)
            material->SetNormal(dynamic_cast<Pine::Texture2D *>(normalResult.asset));

        auto diffuseColor = material->GetDiffuseColor();
        if (Widgets::ColorPicker3("Diffuse Color", diffuseColor))
            material->SetDiffuseColor(diffuseColor);

        auto specularColor = material->GetSpecularColor();
        if (Widgets::ColorPicker3("Specular Color", specularColor))
            material->SetSpecularColor(specularColor);

        auto ambientColor = material->GetAmbientColor();
        if (Widgets::ColorPicker3("Ambient Color", ambientColor))
            material->SetAmbientColor(ambientColor);

        const auto shaderResult = Widgets::AssetPicker("Shader", material->GetShader(), Pine::AssetType::Shader);
        if (shaderResult.hasResult)
            material->SetShader(dynamic_cast<Pine::Shader *>(shaderResult.asset));

        auto shininess = material->GetShininess();
        if (Widgets::SliderFloat("Shininess", &shininess, 0.f, 128.f))
            material->SetShininess(shininess);

        auto textureScale = material->GetTextureScale();
        if (Widgets::SliderFloat("Texture Scale", &textureScale, 0.f, 32.f))
            material->SetTextureScale(textureScale);

        auto renderMode = static_cast<int>(material->GetRenderingMode());
        if (Widgets::Combobox("Rendering Mode", &renderMode, "Opaque\0Discard\0Transparent\0"))
            material->SetRenderingMode(static_cast<Pine::MaterialRenderingMode>(renderMode));
    }

    void RenderModel(Pine::Model *model)
    {

    }

    void RenderBlueprint(Pine::Blueprint *blueprint)
    {

    }

    void RenderLevel(Pine::Level *level)
    {

    }

    void RenderTileset(Pine::Tileset *tileset)
    {
        static int selectedTileset = 0;

        ImGui::Button(ICON_MD_ADD);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drag textures here to add them as tiles.");

        ImGui::SameLine();

        if (ImGui::Button(ICON_MD_REMOVE))
        {
        }

        Widgets::TilesetAtlas(tileset, selectedTileset);

        if (ImGui::Button("Build"))
        {
            tileset->Build();
        }

        ImGui::TextDisabled("Tiles: %d", static_cast<int>(tileset->GetTileList().size()));
        ImGui::TextDisabled("Tile size: %d", tileset->GetTileSize());
    }

    void RenderTilemap(Pine::Tilemap *tilemap)
    {
        const auto newTileset = Widgets::AssetPicker("Tile-set", tilemap->GetTileset(), Pine::AssetType::Tileset);

        if (newTileset.hasResult)
        {
            tilemap->SetTileset(dynamic_cast<Pine::Tileset *>(newTileset.asset));
        }
    }
}

void AssetPropertiesPanel::Render(Pine::IAsset *asset)
{
    auto fileIcon = IconStorage::GetIconTexture(asset->GetPath());

    ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t *>(fileIcon->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(64.f, 64.f));

    ImGui::SameLine();

    ImGui::BeginChild("##AssetPropertiesChild", ImVec2(-1.f, 65.f), false, 0);

    ImGui::Text("%s", asset->GetFileName().c_str());
    ImGui::Text("%s", asset->GetPath().c_str());

    ImGui::Text("%s", AssetTypeToString(asset->GetType()));

    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    switch (asset->GetType())
    {
        case Pine::AssetType::Texture2D:
            RenderTexture2D(dynamic_cast<Pine::Texture2D *>(asset));
            break;
        case Pine::AssetType::Texture3D:
            RenderTexture3D(dynamic_cast<Pine::Texture3D *>(asset));
            break;
        case Pine::AssetType::Shader:
            RenderShader(dynamic_cast<Pine::Shader *>(asset));
            break;
        case Pine::AssetType::Material:
            RenderMaterial(dynamic_cast<Pine::Material *>(asset));
            break;
        case Pine::AssetType::Model:
            RenderModel(dynamic_cast<Pine::Model *>(asset));
            break;
        case Pine::AssetType::Blueprint:
            RenderBlueprint(dynamic_cast<Pine::Blueprint *>(asset));
            break;
        case Pine::AssetType::Level:
            RenderLevel(dynamic_cast<Pine::Level *>(asset));
            break;
        case Pine::AssetType::Tileset:
            RenderTileset(dynamic_cast<Pine::Tileset *>(asset));
            break;
        case Pine::AssetType::Tilemap:
            RenderTilemap(dynamic_cast<Pine::Tilemap *>(asset));
            break;
        default:
            ImGui::Text("No asset properties available.");
            break;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Open File", ImVec2(100.f, 30.f)))
    {

    }

    ImGui::SameLine();

    if (ImGui::Button("Remove", ImVec2(100.f, 30.f)))
    {
        std::filesystem::remove(asset->GetFilePath());

        Selection::Clear();

        Panels::AssetBrowser::RebuildAssetTree();
    }
}
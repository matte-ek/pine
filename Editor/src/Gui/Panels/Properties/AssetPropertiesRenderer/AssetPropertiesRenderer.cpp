#include "AssetPropertiesRenderer.hpp"

#include "IconsMaterialDesign.h"
#include "imgui.h"

#include "Gui/Shared/IconStorage/IconStorage.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"

namespace
{

	void RenderTexture2D(const Pine::Texture2D* texture2d)
	{
		const auto textureAspectRatio = static_cast<float>(texture2d->GetWidth()) / static_cast<float>(texture2d->GetHeight());
		const auto previewWidth = ImGui::GetContentRegionAvail().x * 0.5f;
		const auto previewHeight = previewWidth / textureAspectRatio;

		ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(texture2d->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(previewWidth, previewHeight));

		ImGui::Text("Width: %d", texture2d->GetWidth());
		ImGui::Text("Height: %d", texture2d->GetHeight());
	}

	void RenderTileset(Pine::Tileset* tileset)
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

	void RenderTilemap(Pine::Tilemap* tilemap)
	{
		const auto newTileset = Widgets::AssetPicker("Tile-set", tilemap->GetTileset(), Pine::AssetType::Tileset);

		if (newTileset.hasResult)
		{
			tilemap->SetTileset(dynamic_cast<Pine::Tileset*>(newTileset.asset));
		}
	}
}

void AssetPropertiesPanel::Render(Pine::IAsset* asset)
{
	auto fileIcon = IconStorage::GetIconTexture(asset->GetPath());

	ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(fileIcon->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(64.f, 64.f));

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
		RenderTexture2D(dynamic_cast<Pine::Texture2D*>(asset));
		break;
	case Pine::AssetType::Tileset:
		RenderTileset(dynamic_cast<Pine::Tileset*>(asset));
		break;
	case Pine::AssetType::Tilemap:
		RenderTilemap(dynamic_cast<Pine::Tilemap*>(asset));
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

	}
}
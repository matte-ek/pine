#include "LevelEditor.hpp"

#include "imgui.h"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{

	enum class LevelTypes
	{
		Grass,
		Sand,
		Planet
	};

	// Editor properties

	int m_SelectedLevel = -1;
	LevelTypes m_SelectedLevelType = LevelTypes::Grass;

	bool m_InBuildMode = false;
	bool m_InRemoveMode = false;
	int m_SelectedTile = 0;

	float m_CameraOffset = 0.0f;

	// Engine instances

	Pine::Tilemap* m_Tilemap = nullptr;
	Pine::Tileset* m_Tileset = nullptr;

	Pine::Entity* m_TilemapEntity = nullptr;
	Pine::Entity* m_BackgroundEntity = nullptr;

	Pine::Entity* m_CameraEntity = nullptr;

	// Input data

	bool m_IsMouseDown = false;
	Pine::Vector2f m_CursorPosition;

	void UpdateLevelType()
	{
		auto tilesetFileName = "";
		auto backgroundFileName = "";
		
		switch (m_SelectedLevelType)
		{
		case LevelTypes::Grass:
			tilesetFileName = "grass.tileset";
			backgroundFileName = "Backgrounds/blue_grass.png";
			break;
		case LevelTypes::Sand:
			tilesetFileName = "sand.tileset";
			backgroundFileName = "Backgrounds/colored_desert.png";
			break;
		case LevelTypes::Planet:
			tilesetFileName = "planet.tileset";
			backgroundFileName = "Backgrounds/colored_shroom.png";
			break;
		}

		m_Tileset = Pine::Assets::GetAsset<Pine::Tileset>(tilesetFileName);
		m_Tilemap->SetTileset(m_Tileset);

		m_BackgroundEntity->GetComponent<Pine::SpriteRenderer>()->SetTexture(Pine::Assets::GetAsset<Pine::Texture2D>(backgroundFileName));
	}

	void OnPineRender(Pine::RenderStage stage)
	{
		if (stage != Pine::RenderStage::Render3D)
		{
			return;
		}

		m_CameraEntity->GetTransform()->LocalPosition.x = m_CameraOffset;

		if (!m_InBuildMode)
		{
			return;
		}

		auto selectedTile = Pine::Vector2i(std::floor(m_CursorPosition.x / 64.f), std::floor(m_CursorPosition.y / 64.f));
		auto tileCoordinates = Pine::Vector2f(selectedTile) * 64.f;

		Pine::RenderingContext context = { true, Pine::Vector2f(1280.f, 720.f) };

		Pine::Renderer2D::PrepareFrame();
		Pine::Renderer2D::SetCoordinateSystem(Pine::Rendering::CoordinateSystem::Screen);

		const auto textureAtlas = m_Tileset->GetTextureAtlas();
		for (const auto& tile : m_Tilemap->GetTiles())
		{
            if (!(tile.m_Flags & Pine::NoRender))
                continue;

            Pine::Renderer2D::AddTextureAtlasItem(Pine::Vector2f(64.f * static_cast<float>(tile.m_Position.x),
                                                                 64.f * static_cast<float>(tile.m_Position.y)),
                                                  textureAtlas, tile.m_RenderIndex, Pine::Color(255, 255, 255, 100));
		}

		Pine::Renderer2D::AddFilledRectangle(tileCoordinates, Pine::Vector2f(64.f), Pine::Color(50, 50, 50, 100));

		if (m_IsMouseDown)
		{
			if (const auto tile = m_Tilemap->GetTileByPosition(selectedTile))
			{
				m_Tilemap->RemoveTile(*tile);
			}

			if (!m_InRemoveMode)
			{
				m_Tilemap->CreateTile(static_cast<std::uint32_t>(m_SelectedTile), selectedTile);
			}
		}

		Pine::Renderer2D::RenderFrame(&context);
	}


	void Load()
	{
        const auto existingTilemap = Pine::Assets::GetAsset<Pine::Tilemap>(std::to_string(m_SelectedLevel) + ".tilemap");

	    if (!existingTilemap)
            return;

        for (const auto& tile : existingTilemap->GetTiles())
        {
            m_Tilemap->CreateTile(tile.m_Index, tile.m_Position, tile.m_Flags);
        }
	}

	void Save()
	{
        m_Tilemap->SetFilePath("game/" + std::to_string(m_SelectedLevel) + ".tilemap");

	    m_Tilemap->SaveToFile();
	    m_Tilemap->SaveMetadata();
	}

	void Setup()
	{
		m_BackgroundEntity = Pine::Entity::Create("Background");
		m_BackgroundEntity->AddComponent<Pine::SpriteRenderer>();

		m_TilemapEntity = Pine::Entity::Create("Tilemap");
		m_TilemapEntity->AddComponent<Pine::TilemapRenderer>();

		m_CameraEntity = Pine::Entity::Create("Camera");
        m_CameraEntity->AddComponent<Pine::Camera>();

		m_Tilemap = new Pine::Tilemap;

		UpdateLevelType();

		m_TilemapEntity->GetComponent<Pine::TilemapRenderer>()->SetTilemap(m_Tilemap);
		m_TilemapEntity->GetComponent<Pine::TilemapRenderer>()->SetOrder(-1);

		m_BackgroundEntity->GetTransform()->LocalPosition = Pine::Vector3f(4.f, 0.f, 0.f);
		m_BackgroundEntity->GetTransform()->LocalScale = Pine::Vector3f(8.f, 1.f, 1.f);

		m_BackgroundEntity->GetComponent<Pine::SpriteRenderer>()->SetScalingMode(Pine::SpriteScalingMode::Repeat);
		m_BackgroundEntity->GetComponent<Pine::SpriteRenderer>()->SetOrder(-2);

		Load();

	    Pine::RenderManager::GetPrimaryRenderingContext()->m_Camera = m_CameraEntity->GetComponent<Pine::Camera>();
		Pine::RenderManager::AddRenderCallback(OnPineRender);
	}
}

void LevelEditor::SetLevel(int levelId)
{
	if (m_SelectedLevel == -1)
	{
		m_SelectedLevel = levelId;

		Setup();
	}

	m_SelectedLevel = levelId;
}

void LevelEditor::Render()
{
	ImGui::Begin("Level Editor");

	ImGui::Text("Current Level: %d", m_SelectedLevel);

	int levelType = static_cast<int>(m_SelectedLevelType);
	if (ImGui::Combo("Type", &levelType, "Grass\0Sand\0Planet\0"))
	{
		m_SelectedLevelType = static_cast<LevelTypes>(levelType);

		UpdateLevelType();
	}

    ImGui::Text("Camera Offset");
    ImGui::SliderFloat("##CameraOffset", &m_CameraOffset, 0.f, 8.f);

	if (ImGui::Button("Save"))
	{
		Save();
	}

	ImGui::Separator();

	ImGui::Checkbox("Build Mode", &m_InBuildMode);

	int itemsOnLine = 0;
	for (const auto& tile : m_Tileset->GetTileList())
	{
		std::uint32_t textureId = *static_cast<std::uint32_t*>(tile.m_Texture->GetGraphicsTexture()->GetGraphicsIdentifier());

		if (tile.m_Index != 0 && itemsOnLine < 9)
			ImGui::SameLine();
		else
			itemsOnLine = 0;

		if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(textureId), ImVec2(32.f, 32.f)))
		{
			m_SelectedTile = tile.m_Index;
		}

		itemsOnLine++;
	}

	m_IsMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !(ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered());
	m_InRemoveMode = ImGui::IsKeyDown(ImGuiKey_LeftShift);
	m_CursorPosition = Pine::Vector2f(ImGui::GetMousePos().x, ImGui::GetMousePos().y);

	ImGui::End();
}

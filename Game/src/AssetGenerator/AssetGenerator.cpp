#include "AssetGenerator.hpp"

#include <string>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Pine/Core/String/String.hpp"

#include "Game/Constants.hpp"

namespace
{

	void CreateTilesetFromType(const std::string& typeName)
	{
		if (std::filesystem::exists("game/" + Pine::String::ToLower(typeName) + ".tileset"))
			return;

		auto tileSet = new Pine::Tileset();

		tileSet->SetFilePath("game/" + Pine::String::ToLower(typeName) + ".tileset");
		tileSet->SetTileSize(64);

		const auto texturePrefix = "Ground/" + typeName + "/" + Pine::String::ToLower(typeName);

		// Magic tiles
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("magic-player.png"), Pine::NoCollison | Pine::NoRender | Game::PlayerSpawn);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("magic-coin.png"), Pine::NoCollison | Pine::NoRender | Game::CoinSpawn);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("magic-key.png"), Pine::NoCollison | Pine::NoRender | Game::KeySpawn);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("magic-enemy-1.png"), Pine::NoCollison | Pine::NoRender | Game::EnemySpawn1);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("magic-enemy-2.png"), Pine::NoCollison | Pine::NoRender | Game::EnemySpawn2);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("magic-enemy-3.png"), Pine::NoCollison | Pine::NoRender | Game::EnemySpawn3);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("magic-exit.png"), Pine::NoCollison | Pine::NoRender | Game::ExitSpawn);

		// General tiles
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("Tiles/boxCrate.png"));
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("Tiles/spikes.png"), Pine::NoCollison);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("Tiles/waterTop_high.png"), Pine::NoCollison);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("Tiles/water.png"), Pine::NoCollison);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("Tiles/bush.png"), Pine::NoCollison);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("Tiles/cactus.png"), Pine::NoCollison);
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>("Tiles/signRight.png"), Pine::NoCollison);

		// World tiles
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>(texturePrefix + "Mid.png"));
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>(texturePrefix + "Center.png"));
		tileSet->AddTile(Pine::Assets::GetAsset<Pine::Texture2D>(texturePrefix + "Half.png"));

		tileSet->SaveToFile();
		tileSet->SaveMetadata();

		tileSet->Dispose();

		delete tileSet;
	}

}

void AssetGenerator::Run()
{
	CreateTilesetFromType("Grass");
	CreateTilesetFromType("Sand");
	CreateTilesetFromType("Planet");
}
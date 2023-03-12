#include "IconStorage.hpp"

#include "Pine/Assets/Assets.hpp"

namespace
{

	enum class IconType
	{
		Static,
		Dynamic
	};

	struct Icon
	{
		std::string Path;

		Pine::IAsset* Asset = nullptr;

		Pine::Texture2D* Texture = nullptr;

		IconType Type = IconType::Static;
	};

	Pine::Texture2D* GetStaticIconFromAsset(Pine::IAsset* asset)
	{
		switch (asset->GetType())
		{
		case Pine::AssetType::Texture2D:
			return dynamic_cast<Pine::Texture2D*>(asset);
		case Pine::AssetType::Tileset:
			return Pine::Assets::Get<Pine::Texture2D>("editor/icons/tile-set.png");
		case Pine::AssetType::Tilemap:
			return Pine::Assets::Get<Pine::Texture2D>("editor/icons/tile-map.png");
		default:
			return nullptr;
		}
	}

	std::unordered_map<std::string, Icon> m_IconCache;

}

void IconStorage::Update()
{
	std::vector<std::string> removeList;

	// Find and remove unloaded assets from the icon cache
	for (const auto& [iconAssetPath, icon] : m_IconCache)
	{
		if (Pine::Assets::Get(iconAssetPath))
		{
			continue;
		}

		if (icon.Type == IconType::Dynamic)
		{
			// TODO: Remove generated textures and whatnot.
		}

		removeList.push_back(iconAssetPath);
	}

	for (const auto& icon : removeList)
	{
		m_IconCache.erase(icon);
	}

	// Generate icons
	for (const auto& [path, asset] : Pine::Assets::GetAll())
	{
		Icon* icon = nullptr;

		icon = &m_IconCache[path];

		// If path is empty, it has just been created.
		if (icon->Path.empty())
		{
			icon->Path = path;
			icon->Asset = asset;
		}

		icon->Texture = GetStaticIconFromAsset(asset);
	}
}

Pine::Texture2D* IconStorage::GetIconTexture(const std::string& path)
{
	static auto invalidAssetIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file.png");

	if (!m_IconCache.count(path) || !m_IconCache[path].Texture)
	{
		return invalidAssetIcon;
	}

	return m_IconCache[path].Texture;
}

#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/Color/Color.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include <string>

namespace Pine
{
	class Tileset;
}

struct EntityPickerResult
{
    bool hasResult = false;
    Pine::Entity* entity = nullptr;
};

struct AssetPickerResult
{
    bool hasResult = false;
    Pine::IAsset* asset = nullptr;
};

namespace Widgets
{

    bool Checkbox(const std::string& str, bool* value);

    bool Vector2(const std::string& str, Pine::Vector2f& vector);
    bool Vector3(const std::string& str, Pine::Vector3f& vector, float speed = 0.01f);

    bool Combobox(const std::string& str, int* value, const char* items);

    bool InputInt(const std::string& str, int* value);
    bool InputFloat(const std::string& str, float* value);

    bool SliderFloat(const std::string& str, float* value, float min, float max);

    bool ColorPicker3(const std::string& str, Pine::Vector3f& color);

    AssetPickerResult AssetPicker(const std::string& str, const Pine::IAsset* asset, Pine::AssetType restrictedType = Pine::AssetType::Invalid);
    AssetPickerResult AssetPicker(const std::string& str, const std::string& id, const Pine::IAsset* asset, Pine::AssetType restrictedType = Pine::AssetType::Invalid);

    EntityPickerResult EntityPicker(const std::string& str, const std::string& id, const Pine::Entity* entity);
    EntityPickerResult EntityPicker(const std::string& str, const Pine::Entity* entity);

    bool Icon(const std::string& text, Pine::Graphics::ITexture *texture, bool showBackground, int size = 64);
    bool Icon(const std::string& text, const Pine::Texture2D* texture, bool showBackground, int size = 64);

    void TilesetAtlas(Pine::Tileset* tileset, int& selectedItem);

    void PushDisabled();
    void PopDisabled();

}
#pragma once
#include "Pine/Assets/Asset/Asset.hpp"
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
    Pine::Asset* asset = nullptr;
};

namespace Widgets
{

    bool Checkbox(const std::string& str, bool* value);

    bool Vector2(const std::string& str, Pine::Vector2f& vector, float speed = 0.01f);
    bool Vector3(const std::string& str, Pine::Vector3f& vector, float speed = 0.01f);

    bool DropDown(const std::string& str, int* value, const char* items);

    bool InputInt(const std::string& str, int* value);
    bool InputFloat(const std::string& str, float* value);
    bool InputText(const std::string& str, char* buf, size_t size);

    bool SliderFloat(const std::string& str, float* value, float min, float max);

    bool ColorPicker3(const std::string& str, Pine::Vector3f& color);
    bool ColorPicker4(const std::string& str, Pine::Vector4f& color);

    bool CheckboxVector3(const std::string& str, std::array<bool, 3>& vec);

    AssetPickerResult AssetPicker(const std::string& str, const Pine::Asset* asset, Pine::AssetType restrictedType = Pine::AssetType::Invalid);
    AssetPickerResult AssetPicker(const std::string& str, const std::string& id, const Pine::Asset* asset, Pine::AssetType restrictedType = Pine::AssetType::Invalid);

    EntityPickerResult EntityPicker(const std::string& str, const std::string& id, const Pine::Entity* entity);
    EntityPickerResult EntityPicker(const std::string& str, const Pine::Entity* entity);

    bool Icon(const std::string& text, Pine::Graphics::ITexture *texture, bool showBackground, int size = 64);
    bool Icon(const std::string& text, const Pine::Texture2D* texture, bool showBackground, int size = 64);

    void TilesetAtlas(Pine::Tileset* tileset, int& selectedItem);

    bool LayerSelection(const std::string& text, std::uint32_t& layers);

    void PushDisabled();
    void PopDisabled();

}
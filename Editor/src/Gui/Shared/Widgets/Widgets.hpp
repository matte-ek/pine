#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include <string>

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
    bool Vector3(const std::string& str, Pine::Vector3f& vector);

    bool Combobox(const std::string& str, int* value, const char* items);

    bool InputInt(const std::string& str, int* value);

    AssetPickerResult AssetPicker(const std::string& str, Pine::IAsset* asset, Pine::AssetType restrictedType = Pine::AssetType::Invalid);

    bool Icon(const std::string& text, Pine::Texture2D* texture, bool showBackground, int size = 64);

    void PushDisabled();
    void PopDisabled();

}
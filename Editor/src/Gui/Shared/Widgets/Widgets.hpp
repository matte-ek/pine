#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"
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

    AssetPickerResult AssetPicker(const std::string& str, Pine::IAsset* asset, Pine::AssetType restrictedType = Pine::AssetType::Invalid);

    void PushDisabled();
    void PopDisabled();

}
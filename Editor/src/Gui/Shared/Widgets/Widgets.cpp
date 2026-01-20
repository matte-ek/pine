#include "Widgets.hpp"
#include "IconsMaterialDesign.h"
#include "Pine/Core/Math/Math.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp>

#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"

namespace
{

    void PrepareWidget(const std::string& str)
    {
        ImGui::Columns(2, nullptr, false);

        ImGui::Text("%s", str.c_str());

        ImGui::NextColumn();

        ImGui::BeginChild(std::string(str + "ControlChild").c_str(), ImVec2(-1.f, 28.f), false);
    }

    void FinishWidget()
    {
        ImGui::EndChild();
        ImGui::Columns(1);
    }

}

void Widgets::PushDisabled()
{
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.3f);
}

void Widgets::PopDisabled()
{
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}

bool Widgets::Checkbox(const std::string& str, bool* value)
{
    PrepareWidget(str);

    bool ret = ImGui::Checkbox(fmt::format("##{}", str).c_str(), value);

    FinishWidget();

    return ret;
}

bool Widgets::Vector2(const std::string& str, Pine::Vector2f& vector, float speed)
{
    constexpr float size = 60.f;

    PrepareWidget(str);

    ImGui::SetNextItemWidth(size);
    bool xChanged = ImGui::DragFloat(std::string("X##" + str).c_str(), &vector.x, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10.f, 1.f));
    ImGui::SameLine();

    ImGui::SetNextItemWidth(size);
    bool yChanged = ImGui::DragFloat(std::string("Y##" + str).c_str(), &vector.y, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    FinishWidget();

    return xChanged || yChanged;
}

bool Widgets::Vector3(const std::string& str, Pine::Vector3f& vector, float speed)
{
    constexpr float size = 60.f;

    PrepareWidget(str);

    ImGui::Columns(3, nullptr, false);

    ImGui::SetNextItemWidth(size);
    bool xChanged = ImGui::DragFloat(std::string("X##" + str).c_str(), &vector.x, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    ImGui::NextColumn();

    ImGui::SetNextItemWidth(size);
    bool yChanged = ImGui::DragFloat(std::string("Y##" + str).c_str(), &vector.y, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    ImGui::NextColumn();

    ImGui::SetNextItemWidth(size);
    bool zChanged = ImGui::DragFloat(std::string("Z##" + str).c_str(), &vector.z, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    FinishWidget();

    return xChanged || yChanged || zChanged;
}

bool Widgets::Combobox(const std::string& str, int* value, const char* items)
{
    bool ret = false;
    
    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::Combo(std::string("##Combo" + str).c_str(), value, items);

    FinishWidget();

    return ret;
}

bool Widgets::InputInt(const std::string& str, int* value)
{
    bool ret = false;
    
    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::InputInt(std::string("##InputInt" + str).c_str(), value);

    FinishWidget();

    return ret;
}

bool Widgets::InputFloat(const std::string& str, float* value)
{
    bool ret = false;
    
    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::InputFloat(std::string("##InputFloat" + str).c_str(), value);

    FinishWidget();

    return ret;
}

bool Widgets::InputText(const std::string& str, char* buf, size_t size)
{
    bool ret = false;

    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::InputText(std::string("##InputText" + str).c_str(), buf, size);

    FinishWidget();

    return ret;
}

bool Widgets::SliderFloat(const std::string& str, float* value, float min, float max)
{
    bool ret = false;
    
    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::SliderFloat(std::string("##SliderFloat" + str).c_str(), value, min, max);

    FinishWidget();

    return ret;
}

bool Widgets::ColorPicker3(const std::string& str, Pine::Vector3f& color)
{
    bool ret = false;
    
    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::ColorEdit3(std::string("##ColorPicker3" + str).c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoAlpha);

    FinishWidget();

    return ret;
}

bool Widgets::CheckboxVector3(const std::string& str, std::array<bool, 3>& vec)
{
    bool ret = false;

    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret |= ImGui::Checkbox("X", &vec[0]);

    ImGui::SameLine();

    ret |= ImGui::Checkbox("Y", &vec[1]);

    ImGui::SameLine();

    ret |= ImGui::Checkbox("Z", &vec[2]);

    FinishWidget();

    return ret;
}

AssetPickerResult Widgets::AssetPicker(const std::string& str, const Pine::IAsset* asset, Pine::AssetType restrictedType)
{
    return AssetPicker(str, "", asset, restrictedType);
}

AssetPickerResult Widgets::AssetPicker(const std::string& str, const std::string& id, const Pine::IAsset* asset, Pine::AssetType restrictedType)
{
    AssetPickerResult ret;

    std::string assetFileName;

    if (asset != nullptr)
    {
       assetFileName = asset->GetFileName();
    }

    if (assetFileName.size() > 128)
    {
        return ret;
    }

    if (!id.empty())
        ImGui::PushID(id.c_str());

    PrepareWidget(str);

    char buff[128];

    strcpy(buff, assetFileName.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.f);

    ImGui::InputText(std::string("##AssetPath" + str).c_str(), buff, 128, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::IsItemClicked() && asset != nullptr)
    {
        Selection::Add(const_cast<Pine::IAsset*>(asset), true);
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const auto payload = ImGui::AcceptDragDropPayload("Asset"))
        {
            auto droppedAsset = *static_cast<Pine::IAsset**>(payload->Data);

            if (droppedAsset && (restrictedType == Pine::AssetType::Invalid || droppedAsset->GetType() == restrictedType))
            {
                ret.hasResult = true;
                ret.asset = droppedAsset;
            }
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();

    if (ImGui::Button(" ... "))
    {
        // TODO: Asset picker
    }

    ImGui::SameLine();

    if (asset == nullptr)
    {
        Widgets::PushDisabled();
    }

    if (ImGui::Button(ICON_MD_DELETE))
    {
        ret.hasResult = true;
        ret.asset = nullptr;
    }

    if (asset == nullptr)
    {
        Widgets::PopDisabled();
    }

    FinishWidget();

    if (!id.empty())
        ImGui::PopID();

    return ret;
}

bool Widgets::Icon(const std::string& text, const Pine::Texture2D* texture, bool showBackground, int size)
{
    return Icon(text, texture->GetGraphicsTexture(), showBackground, size);
}

bool Widgets::Icon(const std::string& text, Pine::Graphics::ITexture *texture, bool showBackground, int size)
{
    bool ret = false;

    ImGui::PushID(text.c_str());
    ImGui::BeginGroup();

    if (!showBackground)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    }

    std::uint64_t textureId = *static_cast<std::uint32_t*>(texture->GetGraphicsIdentifier());

    if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(textureId), ImVec2(static_cast<float>(size), static_cast<float>(size)),
                            ImVec2(0, 0), ImVec2(1, 1), 3))
    {
        ret = true;
    }

    if (!showBackground)
    {
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (static_cast<float>(size) / 2.f - (std::min(ImGui::CalcTextSize(text.c_str()).x, static_cast<float>(size)) / 2.f)) + 2.f);
    ImGui::TextWrapped("%s", text.c_str());

    ImGui::EndGroup();
    ImGui::PopID();

    return ret;
}

void Widgets::TilesetAtlas(Pine::Tileset* tileset, int& selectedItem)
{
    constexpr auto tileSize = 48.f;

    const auto atlasPreviewSize = ImVec2(ImGui::GetContentRegionAvail().x, 150.f);

    ImGui::BeginChild("TilesetAtlas", ImVec2(-1, atlasPreviewSize.y), true);
    {
        const int nmColumns = static_cast<int>(std::floor((ImGui::GetContentRegionAvail().x - 128.f) / tileSize));

        ImGui::Columns(nmColumns, nullptr, false);

        int index = 0;
        for (const auto& tile : tileset->GetTileList())
        {
            bool restoreBackground = false;

            if (index != selectedItem)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                restoreBackground = true;
            }

            if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(tile.m_Texture->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(tileSize, tileSize)))
            {
                selectedItem = index;
            }

            if (restoreBackground)
            {
                ImGui::PopStyleColor();
            }

            ImGui::NextColumn();

            index++;
        }

        ImGui::Columns(1);
    }
    ImGui::EndChild();
}

EntityPickerResult Widgets::EntityPicker(const std::string &str, const std::string &id, const Pine::Entity *entity)
{
    EntityPickerResult ret;

    if (!id.empty())
        ImGui::PushID(id.c_str());

    std::string entityName;

    if (entity != nullptr && entity->GetName().size() < 128)
    {
        entityName = entity->GetName();
    }

    PrepareWidget(str);

    char buff[128];

    strcpy(buff, entityName.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.f);

    ImGui::InputText(std::string("##EntityName" + str).c_str(), buff, 128, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::IsItemClicked() && entity != nullptr)
    {
        Selection::Add(const_cast<Pine::Entity*>(entity), true);
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const auto payload = ImGui::AcceptDragDropPayload("Entity"))
        {
            auto droppedEntity = *static_cast<Pine::Entity**>(payload->Data);

            if (droppedEntity)
            {
                ret.hasResult = true;
                ret.entity = droppedEntity;
            }
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();

    if (ImGui::Button(" ... "))
    {
        // TODO: Entity picker
    }

    ImGui::SameLine();

    if (entity == nullptr)
    {
        Widgets::PushDisabled();
    }

    if (ImGui::Button(ICON_MD_DELETE))
    {
        ret.hasResult = true;
        ret.entity = nullptr;
    }

    if (entity == nullptr)
    {
        Widgets::PopDisabled();
    }

    FinishWidget();

    if (!id.empty())
        ImGui::PopID();

    return ret;
}

EntityPickerResult Widgets::EntityPicker(const std::string &str, const Pine::Entity *entity)
{
    return EntityPicker(str, "", entity);
}

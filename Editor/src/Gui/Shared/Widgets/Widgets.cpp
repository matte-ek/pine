#include "Widgets.hpp"
#include "IconsMaterialDesign.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <cstring>

#include "Pine/Assets/Tileset/Tileset.hpp"

namespace
{

    void PrepareWidget(const std::string& str)
    {
        ImGui::Columns(2, nullptr, false);

        ImGui::Text("%s", str.c_str());

        ImGui::NextColumn();

        ImGui::BeginChild(std::string(str + "ControlChild").c_str(), ImVec2(-1.f, 25.f), false);
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

    bool ret = ImGui::Checkbox(str.c_str(), value);

    FinishWidget();

    return ret;
}

bool Widgets::Vector2(const std::string& str, Pine::Vector2f& vector)
{
    return false;
}

bool Widgets::Vector3(const std::string& str, Pine::Vector3f& vector)
{
    constexpr float size = 50.f;

    PrepareWidget(str);

    ImGui::Columns(3, nullptr, false);

    ImGui::SetNextItemWidth(size);
    bool xChanged = ImGui::DragFloat(std::string("X##" + str).c_str(), &vector.x, 0.01f, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput);

    ImGui::NextColumn();

    ImGui::SetNextItemWidth(size);
    bool yChanged = ImGui::DragFloat(std::string("Y##" + str).c_str(), &vector.y, 0.01f, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput);

    ImGui::NextColumn();

    ImGui::SetNextItemWidth(size);
    bool zChanged = ImGui::DragFloat(std::string("Z##" + str).c_str(), &vector.z, 0.01f, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput);

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

AssetPickerResult Widgets::AssetPicker(const std::string& str, const Pine::IAsset* asset, Pine::AssetType restrictedType)
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

    PrepareWidget(str);

    char buff[128];

    strcpy_s(buff, assetFileName.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 51.f);

    ImGui::InputText(std::string("##AssetPath" + str).c_str(), buff, 128, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::BeginDragDropTarget())
    {
        if (const auto payload = ImGui::AcceptDragDropPayload("Asset"))
        {
            auto droppedAsset = *static_cast<Pine::IAsset**>(payload->Data);

            if (restrictedType == Pine::AssetType::Invalid || droppedAsset->GetType() == restrictedType)
            {
                ret.hasResult = true;
                ret.asset = droppedAsset;
            }
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();

    if (ImGui::Button("..."))
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

        Widgets::PushDisabled();
    }

    if (asset == nullptr)
    {
        Widgets::PopDisabled();
    }

    FinishWidget();

    return ret;
}

bool Widgets::Icon(const std::string& text, const Pine::Texture2D* texture, bool showBackground, int size)
{
    bool ret = false;

    ImGui::PushID(text.c_str());
    ImGui::BeginGroup();

    if (!showBackground)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    }

    std::uint64_t textureId = *static_cast<std::uint32_t*>(texture->GetGraphicsTexture()->GetGraphicsIdentifier());

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
    constexpr auto tileSize = 32.f;

    const auto atlasPreviewSize = ImVec2(ImGui::GetContentRegionAvail().x - 128.f, 150.f);
    const int nmColumns = static_cast<int>(std::floor(atlasPreviewSize.x / tileSize));

    ImGui::BeginChild("TilesetAtlas", ImVec2(-1.f, atlasPreviewSize.y), true);
    {
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

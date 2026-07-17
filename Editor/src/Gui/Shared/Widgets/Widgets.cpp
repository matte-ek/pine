#include "Widgets.hpp"
#include "IconsMaterialDesign.h"
#include "Pine/Core/Math/Math.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp>

#include "Gui/Gui.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Pine/Game/Game.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"

namespace
{

    void PrepareWidget(const std::string& str)
    {
        ImGui::Columns(2, nullptr, false);

        ImGui::Text("%s", str.c_str());

        ImGui::NextColumn();

        ImGui::BeginChild(std::string(str + "ControlChild").c_str(), ImVec2(-1.f, 30.f), false);
    }

    void FinishWidget()
    {
        ImGui::EndChild();
        ImGui::Columns(1);
    }

    void CoordinateText(const char* coordinateText, ImColor color, ImColor textColor)
    {
        const auto cursorPosition = ImGui::GetCursorScreenPos();
        const auto frameSize = ImGui::GetFrameHeight();
        const auto rounding = ImGui::GetStyle().FrameRounding;

        //ImGui::PushFont(Editor::Gui::GetBoldFont());

        const auto textSize = ImGui::CalcTextSize(coordinateText);

        ImGui::GetWindowDrawList()->AddRectFilled(cursorPosition, ImVec2(cursorPosition.x + frameSize, cursorPosition.y + frameSize), color, rounding);
        ImGui::GetWindowDrawList()->AddText(ImVec2(cursorPosition.x + (frameSize / 2.f) - (textSize.x / 2.f), cursorPosition.y + (frameSize / 2.f) - (textSize.y / 2.f) - 1), textColor, coordinateText);
        ImGui::Dummy(ImVec2(frameSize, frameSize));
        ImGui::SameLine();

        //ImGui::PopFont();
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

void Widgets::Text(const std::string& str, const std::string& text)
{
    PrepareWidget(str);

    ImGui::Text("%s", text.c_str());

    FinishWidget();
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

bool Widgets::Vector2i(const std::string& str, Pine::Vector2i& vector, float speed)
{
    constexpr float size = 60.f;

    PrepareWidget(str);

    CoordinateText("X", ImColor(150, 34, 34), ImColor(150, 34, 34));

    ImGui::SetNextItemWidth(size);
    bool xChanged = ImGui::DragInt(std::string("X##" + str).c_str(), &vector.x, speed, -INT_MAX, INT_MAX, "%d", ImGuiSliderFlags_AlwaysClamp);

    ImGui::SameLine(0.f, 10.f);

    CoordinateText("Y", ImColor(30, 62, 30, 100), ImColor(11, 255, 11));

    ImGui::SetNextItemWidth(size);
    bool yChanged = ImGui::DragInt(std::string("Y##" + str).c_str(), &vector.y, speed, -INT_MAX, INT_MAX, "%d", ImGuiSliderFlags_AlwaysClamp);

    FinishWidget();

    return xChanged || yChanged;
}

bool Widgets::Vector3(const std::string& str, Pine::Vector3f& vector, float speed)
{
    constexpr float size = 55.f;

    PrepareWidget(str);

    ImGui::Columns(3, nullptr, false);

    CoordinateText("X", ImColor(62, 30, 30, 100), ImColor(255, 11, 11));

    ImGui::SetNextItemWidth(size);
    bool xChanged = ImGui::DragFloat(std::string("##X" + str).c_str(), &vector.x, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    ImGui::NextColumn();

    CoordinateText("Y", ImColor(30, 62, 30, 100), ImColor(11, 255, 11));

    ImGui::SetNextItemWidth(size);
    bool yChanged = ImGui::DragFloat(std::string("##Y" + str).c_str(), &vector.y, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    ImGui::NextColumn();

    CoordinateText("Z", ImColor(30, 30, 62, 100), ImColor(11, 150, 255));

    ImGui::SetNextItemWidth(size);
    bool zChanged = ImGui::DragFloat(std::string("##Z" + str).c_str(), &vector.z, speed, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    FinishWidget();

    return xChanged || yChanged || zChanged;
}

bool Widgets::DropDown(const std::string& str, int* value, const char* items)
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

bool Widgets::SliderInt(const std::string& str, int* value, int min, int max)
{
    bool ret = false;

    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::SliderInt(std::string("##SliderInt" + str).c_str(), value, min, max);

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

bool Widgets::ColorPicker4(const std::string& str, Pine::Vector4f& color)
{
    bool ret = false;

    PrepareWidget(str);

    ImGui::SetNextItemWidth(-1.f);

    ret = ImGui::ColorEdit4(std::string("##ColorPicker4" + str).c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoAlpha);

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

AssetPickerResult Widgets::AssetPicker(const std::string& str, const Pine::Asset* asset, Pine::AssetType restrictedType)
{
    return AssetPicker(str, "", asset, restrictedType);
}

AssetPickerResult Widgets::AssetPicker(const std::string& str, const std::string& id, const Pine::Asset* asset, Pine::AssetType restrictedType)
{
    AssetPickerResult ret;

    std::string assetFileName;

    if (asset != nullptr)
    {
       assetFileName = std::filesystem::path(asset->GetPath()).filename().string();
    }

    if (assetFileName.size() > 128)
    {
        return ret;
    }

    if (!id.empty())
    {
        ImGui::PushID(id.c_str());
    }

    PrepareWidget(str);

    char buff[128];

    strcpy(buff, assetFileName.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.f);

    ImGui::InputText(std::string("##AssetPath" + str).c_str(), buff, 128, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::IsItemClicked() && asset != nullptr)
    {
        Selection::Add(const_cast<Pine::Asset*>(asset), true);
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const auto payload = ImGui::AcceptDragDropPayload("Asset"))
        {
            auto droppedAsset = *static_cast<Pine::Asset**>(payload->Data);

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
        PushDisabled();
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

int Widgets::AssetIcon(const std::string& text, const Pine::Texture2D* texture, bool showBackground, const char* bottomText, int size)
{
    return AssetIcon(text, texture->GetGraphicsTexture(), showBackground, bottomText, size);
}

int Widgets::AssetIcon(const std::string& text, Pine::Graphics::ITexture *texture, bool showBackground, const char* bottomText, int size)
{
    int ret = 0;

    const auto beginScreenPos = ImGui::GetCursorPos();
    const auto beginCursorScreenPos = ImGui::GetCursorScreenPos();

    const bool isHovering = ImGui::IsMouseHoveringRect(
        beginCursorScreenPos,
        ImVec2(beginCursorScreenPos.x + 128, beginCursorScreenPos.y + 140));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);

    ImGui::PushStyleColor(ImGuiCol_Border, showBackground ? ImVec4(0.26f, 0.75f, 0.45f, 1.00f) : ImVec4(0.081f, 0.132f, 0.075f, 1.000f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, isHovering || showBackground ? ImVec4(0.11f, 0.15f, 0.14f, 1.00f) : ImVec4(0.054f, 0.068f, 0.061f, 1.000f));

    ImGui::SetNextItemAllowOverlap();

    ImGui::BeginChild(text.c_str(), ImVec2(128, 140), ImGuiChildFlags_Borders | ImGuiChildFlags_FrameStyle);
    ImGui::Spacing();
    ImGui::Spacing();

    if (!showBackground)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    }

    const std::uint64_t textureId = *static_cast<std::uint32_t*>(texture->GetGraphicsIdentifier());

    ImGui::SetCursorPosX(32.f);

    ImGui::Image(textureId, ImVec2(static_cast<float>(size), static_cast<float>(size)), ImVec2(0, 0), ImVec2(1, 1));

    if (!showBackground)
    {
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (static_cast<float>(128) / 2.f - (std::min(ImGui::CalcTextSize(text.c_str()).x, static_cast<float>(128)) / 2.f)) + 2.f);
    ImGui::Text("%s", text.c_str());

    if (bottomText != nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (static_cast<float>(128) / 2.f - (std::min(ImGui::CalcTextSize(bottomText).x, static_cast<float>(128)) / 2.f)) + 2.f);
        ImGui::Text("%s", bottomText);
        ImGui::PopStyleColor();
    }

    ImGui::SetNextItemAllowOverlap();

    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(beginScreenPos);

    if (ImGui::InvisibleButton(std::string(text + "Btn").c_str(), ImVec2(128.f, 140.f), ImGuiButtonFlags_FlattenChildren))
    {
        ret = 1;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && isHovering)
    {
        ret = 2;
    }

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

            if (ImGui::ImageButton("", *static_cast<std::uint64_t*>(tile.m_Texture->GetGraphicsTexture()->GetGraphicsIdentifier()), ImVec2(tileSize, tileSize)))
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

bool Widgets::LayerSelection(const std::string& text, std::uint32_t& layers)
{
    static std::string previewTextBuffer;

    bool ret = false;

    PrepareWidget(text);

    ImGui::SetNextItemWidth(-1.f);

    // Build preview text
    if (layers == 0)
    {
        previewTextBuffer = "Nothing";
    }
    else if (layers == 0xFFFFFFFF)
    {
        previewTextBuffer = "Everything";
    }
    else
    {
        previewTextBuffer = "";

        if (layers & Pine::ColliderLayerDefault)
        {
            previewTextBuffer = "Default";
        }

        for (int i = 0; i < 31; i++)
        {
            if (layers & (1 << (i + 1)))
            {
                if (!previewTextBuffer.empty())
                {
                    previewTextBuffer += ", ";
                }

                previewTextBuffer += Pine::Game::GetGameProperties().ColliderLayers[i];
            }
        }
    }

    if (ImGui::BeginCombo(text.c_str(), previewTextBuffer.c_str(), 0))
    {
        // Always show built in default layer
        if (ImGui::Selectable("Default", layers & (1 << 0)))
        {
            if (layers & (1 << 0))
            {
                layers &= ~(1 << 0);
            }
            else
            {
                layers |= (1 << 0);
            }

            ret = true;
        }

        for (int i = 0; i < 31; i++)
        {
            bool selected = layers & (1 << (i + 1));

            if (Pine::Game::GetGameProperties().ColliderLayers[i].empty())
            {
                continue;
            }

            if (ImGui::Selectable(Pine::Game::GetGameProperties().ColliderLayers[i].c_str(), &selected))
            {
                if (selected)
                {
                    layers |= (1 << (i + 1));
                }
                else
                {
                    layers &= ~(1 << (i + 1));
                }

                ret = true;
            }
        }

        ImGui::EndCombo();
    }

    FinishWidget();

    return ret;
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

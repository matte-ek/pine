#include "Widgets.hpp"
#include "imgui_internal.h"

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
    const float size = 50.f;

    PrepareWidget(str);

    ImGui::Columns(3, nullptr, false);

    ImGui::SetNextItemWidth(size);
    bool xChanged = ImGui::DragFloat(std::string("X##" + str).c_str(), &vector.x, 0.1f, FLT_MIN, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput);

    ImGui::NextColumn();

    ImGui::SetNextItemWidth(size);
    bool yChanged = ImGui::DragFloat(std::string("Y##" + str).c_str(), &vector.y, 0.1f, FLT_MIN, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput);

    ImGui::NextColumn();

    ImGui::SetNextItemWidth(size);
    bool zChanged = ImGui::DragFloat(std::string("Z##" + str).c_str(), &vector.z, 0.1f, FLT_MIN, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput);

    FinishWidget();

    return xChanged || yChanged || zChanged;
}

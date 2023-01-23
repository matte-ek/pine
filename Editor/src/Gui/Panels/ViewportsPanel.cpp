#include "Panels.hpp"
#include "imgui.h"

void Panels::ShowViewports()
{

    ImGui::Begin("Level", &Panels::State::Level);
    {

    }
    ImGui::End();

    ImGui::Begin("Game", &Panels::State::Game);
    {

    }
    ImGui::End();

}
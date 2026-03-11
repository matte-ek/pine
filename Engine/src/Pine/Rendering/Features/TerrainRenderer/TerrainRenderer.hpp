#pragma once
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"

namespace Pine
{
    class Camera;
}

namespace Pine::Rendering::TerrainRenderer
{
    void Setup();
    void Shutdown();

    void NewFrame(Camera* sceneCamera);
    void Render(const TerrainRendererComponent* terrainRendererComponent);
}

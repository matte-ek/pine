#pragma once
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"

namespace Pine::Rendering::TerrainRenderer
{
    void Setup();
    void Shutdown();

    void Render(const TerrainRendererComponent* terrainRendererComponent);
}

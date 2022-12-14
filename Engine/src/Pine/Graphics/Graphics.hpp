#pragma once
#include "Pine/Graphics/Interfaces/IGraphicsAPI.hpp"

namespace Pine::Graphics
{
    enum class GraphicsAPI
    {
        OpenGL,
        Vulkan
    };

    // Attempt to create and initialize the specified graphics API
    // returns false on failure.
    bool Setup(GraphicsAPI api);

    // Call before application exit, to free GPU resources
    void Shutdown();

    bool HasInitializedGraphicsAPI();
    IGraphicsAPI* GetGraphicsAPI();
}
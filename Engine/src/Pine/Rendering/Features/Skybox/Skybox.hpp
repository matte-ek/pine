#pragma once
#include "Pine/Assets/Texture3D/Texture3D.hpp"

namespace Pine::Renderer::Skybox
{
	void Setup();
	void Shutdown();

    void Render(Pine::Texture3D* cubeMap);
}
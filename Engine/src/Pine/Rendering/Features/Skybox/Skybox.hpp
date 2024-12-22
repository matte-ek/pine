#pragma once
#include "Pine/Assets/Texture3D/Texture3D.hpp"

namespace Pine::Renderer::Skybox
{
	void Setup();
	void Shutdown();

    void Render(const Texture3D* cubeMap);
}
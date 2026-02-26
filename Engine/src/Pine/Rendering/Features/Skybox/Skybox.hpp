#pragma once
#include "Pine/Assets/Texture3D/Texture3D.hpp"

namespace Pine::Rendering::Skybox
{
	void Setup();
	void Shutdown();

    void Render(Texture3D* cubeMap);
}
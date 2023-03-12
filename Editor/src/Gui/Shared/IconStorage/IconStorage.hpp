#pragma once
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace IconStorage
{

	void Update();

	Pine::Texture2D* GetIconTexture(const std::string& path);

}
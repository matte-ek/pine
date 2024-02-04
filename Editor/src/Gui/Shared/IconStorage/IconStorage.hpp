#pragma once
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace IconStorage
{

    void Setup();
	void Update();

    void MarkIconDirty(const std::string& path);

	Pine::Graphics::ITexture* GetIconTexture(const std::string& path);

}

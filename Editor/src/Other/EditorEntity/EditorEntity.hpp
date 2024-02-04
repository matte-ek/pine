#pragma once
#include "Pine/World/Entity/Entity.hpp"

namespace EditorEntity
{

	void Setup();
	void Dispose();

    void SetCaptureMouse(bool value);

	Pine::Entity* Get();

}

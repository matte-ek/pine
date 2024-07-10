#pragma once
#include "Pine/World/Entity/Entity.hpp"

namespace EditorEntity
{

	void Setup();
	void Dispose();

    bool GetCaptureMouse();
    void SetCaptureMouse(bool value);

    bool GetPerspective2D();
    void SetPerspective2D(bool value);

	Pine::Entity* Get();

}

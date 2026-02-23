#pragma once
#include "Pine/World/Entity/Entity.hpp"

namespace Editor::LevelEntity
{

	void Setup();
	void Dispose();

    bool GetCaptureMouse();
    void SetCaptureMouse(bool value);

    bool GetPerspective2D();
    void SetPerspective2D(bool value);

	float GetSpeedMultiplier();
	void SetSpeedMultiplier(float value);

	Pine::Entity* Get();

}

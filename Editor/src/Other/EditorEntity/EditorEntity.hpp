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

    // Repositions the fly camera and synchronizes its navigation state. Rotation must be a
    // normalized quaternion. Clears residual movement so the supplied view stays in place.
    void SetView(const Pine::Vector3f& position, const Pine::Quaternion& rotation);

	float GetSpeedMultiplier();
	void SetSpeedMultiplier(float value);

	Pine::Entity* Get();

}

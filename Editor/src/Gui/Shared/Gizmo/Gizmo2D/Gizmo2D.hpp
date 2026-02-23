#pragma once
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Editor::Gui::Gizmo::Gizmo2D
{
    void Setup();

	void Render(const Pine::Vector2f& position, const Pine::Vector2f& size);
}

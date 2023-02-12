#pragma once
#include "Pine/Core/Math/Math.hpp"
#include <string>

namespace Widgets
{

    bool Checkbox(const std::string& str, bool* value);

    bool Vector2(const std::string& str, Pine::Vector2f& vector);
    bool Vector3(const std::string& str, Pine::Vector3f& vector);

    void PushDisabled();
    void PopDisabled();

}
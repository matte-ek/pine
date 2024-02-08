#include "Interfaces.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/Input/Input.hpp"
#include <mono/metadata/appdomain.h>

// NOTICE: Storing and using the array index as an handle is okay here, as all the inputs are
// read-only in the scripts and will be looked up on each execution. However if inputs ever become
// dynamic from the script this will cause nasty bugs.

namespace
{
    bool IsKeyDown(int key)
    {
        return Pine::Input::IsKeyDown(static_cast<Pine::KeyCode>(key));
    }

    bool IsMouseButtonDown(int key)
    {
        return Pine::Input::IsMouseButtonDown(static_cast<Pine::KeyCode>(key));
    }

    int GetKeyState(int key)
    {
        return static_cast<int>(Pine::Input::GetKeyState(static_cast<Pine::KeyCode>(key)));
    }

    int GetMouseButtonKeyState(int key)
    {
        return static_cast<int>(Pine::Input::GetMouseButtonState(static_cast<Pine::MouseButton>(key)));
    }

    void GetMousePosition(Pine::Vector2f* mousePosition)
    {
        *mousePosition = Pine::Input::GetCursorPosition();
    }

}

void Pine::Script::Interfaces::Component::Setup()
{
    mono_add_internal_call("Pine.Input.InputManager::PineIsKeyDown", reinterpret_cast<void *>(IsKeyDown));
    mono_add_internal_call("Pine.Input.InputManager::PineIsMouseButtonDown", reinterpret_cast<void *>(IsMouseButtonDown));
    mono_add_internal_call("Pine.Input.InputManager::PineGetKeyState", reinterpret_cast<void *>(GetKeyState));
    mono_add_internal_call("Pine.Input.InputManager::PineGetMouseButtonKeyState", reinterpret_cast<void *>(GetMouseButtonKeyState));
    mono_add_internal_call("Pine.Input.InputManager::PineGetMousePosition", reinterpret_cast<void *>(GetMousePosition));
}
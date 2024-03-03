#include "Interfaces.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/Input/Input.hpp"
#include <mono/metadata/appdomain.h>

// NOTICE: Storing and using the array index as a handle is okay here, as all the inputs are
// read-only in the scripts and will be looked up on each execution. However, if inputs ever become
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

    int LookupInputBind(MonoString* name)
    {
        const auto context = Pine::Input::GetDefaultContext();
        const std::string strName = mono_string_to_utf8(name);

        for (size_t i = 0; i < context->InputBindings.size(); i++)
        {
            if (context->InputBindings[i]->GetName() == strName)
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    int GetInputBindType(int handle)
    {
        return static_cast<int>(Pine::Input::GetDefaultContext()->InputBindings[handle]->GetType());
    }

    MonoString* GetInputBindName(int handle)
    {
        return mono_string_new(mono_domain_get(), Pine::Input::GetDefaultContext()->InputBindings[handle]->GetName().c_str());
    }

    float GetInputBindAxisValue(int handle)
    {
        return Pine::Input::GetDefaultContext()->InputBindings[handle]->GetAxisValue();
    }

    bool PollInputBindActionState(int handle)
    {
        return Pine::Input::GetDefaultContext()->InputBindings[handle]->PollActionState();
    }

}

void Pine::Script::Interfaces::Input::Setup()
{
    mono_add_internal_call("Pine.Input.InputManager::PineIsKeyDown", reinterpret_cast<void *>(IsKeyDown));
    mono_add_internal_call("Pine.Input.InputManager::PineIsMouseButtonDown", reinterpret_cast<void *>(IsMouseButtonDown));
    mono_add_internal_call("Pine.Input.InputManager::PineGetKeyState", reinterpret_cast<void *>(GetKeyState));
    mono_add_internal_call("Pine.Input.InputManager::PineGetMouseButtonKeyState", reinterpret_cast<void *>(GetMouseButtonKeyState));
    mono_add_internal_call("Pine.Input.InputManager::PineGetMousePosition", reinterpret_cast<void *>(LookupInputBind));
    mono_add_internal_call("Pine.Input.InputManager::PineFindInputBinding", reinterpret_cast<void *>(GetMousePosition));
    mono_add_internal_call("Pine.Input.InputManager::PineGetInputBindType", reinterpret_cast<void *>(GetInputBindType));
    mono_add_internal_call("Pine.Input.InputManager::PineGetInputBindName", reinterpret_cast<void *>(GetInputBindName));
    mono_add_internal_call("Pine.Input.InputManager::PineGetInputBindAxisValue", reinterpret_cast<void *>(GetInputBindAxisValue));
    mono_add_internal_call("Pine.Input.InputManager::PinePollInputBindActionState", reinterpret_cast<void *>(PollInputBindActionState));
}
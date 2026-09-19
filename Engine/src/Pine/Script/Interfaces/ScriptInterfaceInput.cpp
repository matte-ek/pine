#include "Interfaces.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/Input/Input.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"

// NOTICE: Storing and using the array index as a handle is okay here, as all the inputs are
// read-only in the scripts and will be looked up on each execution. However, if inputs ever become
// dynamic from the script this will cause nasty bugs.

namespace
{
    // Begin InputBind functions

    void AddKeyboardBinding(const int bindingIndex, const int key, const float value = 1.f)
    {
        Pine::Input::GetDefaultContext()->InputBindings[bindingIndex]->AddKeyboardBinding(key, value);
    }

    void AddAxisBinding(const int bindingIndex, int axis, const float sensitivity = 1.f)
    {
        Pine::Input::GetDefaultContext()->InputBindings[bindingIndex]->AddAxisBinding(static_cast<Pine::Axis>(axis), sensitivity);
    }

    // End InputBind functions

    int CreateInputBinding(const char* name, const Pine::InputType type)
    {
        Pine::Input::GetDefaultContext()->CreateInputBinding(name, type);
        return static_cast<int>(Pine::Input::GetDefaultContext()->InputBindings.size() - 1);
    }

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

    int GetMouseButtonState(int key)
    {
        return static_cast<int>(Pine::Input::GetMouseButtonState(static_cast<Pine::MouseButton>(key)));
    }

    void GetMousePosition(Pine::Vector2f* mousePosition)
    {
        *mousePosition = Pine::Input::GetCursorPosition();
    }

    void GetMouseDelta(Pine::Vector2f* mouseDelta)
    {
        *mouseDelta = Pine::Input::GetMouseDelta();
    }

    void SetCursorMode(const int mode)
    {
        Pine::Input::SetCursorMode(static_cast<Pine::CursorMode>(mode));
    }

    int LookupInputBind(const char* name)
    {
        const auto context = Pine::Input::GetDefaultContext();

        for (size_t i = 0; i < context->InputBindings.size(); i++)
        {
            if (context->InputBindings[i]->GetName() == name)
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    int GetInputBindType(const int handle)
    {
        return static_cast<int>(Pine::Input::GetDefaultContext()->InputBindings[handle]->GetType());
    }

    const char* GetInputBindName(const int handle)
    {
        return Pine::Script::Bindings::ReturnString(
            Pine::Input::GetDefaultContext()->InputBindings[handle]->GetName());
    }

    float GetInputBindAxisValue(const int handle)
    {
        return Pine::Input::GetDefaultContext()->InputBindings[handle]->GetAxisValue();
    }

    bool PollInputBindActionState(const int handle)
    {
        return Pine::Input::GetDefaultContext()->InputBindings[handle]->PollActionState();
    }

}

void Pine::Script::Interfaces::Input::Setup()
{
    // InputManager
    Bindings::Register("Pine.Input.InputManager::PineIsKeyDown", IsKeyDown);
    Bindings::Register("Pine.Input.InputManager::PineIsMouseButtonDown", IsMouseButtonDown);
    Bindings::Register("Pine.Input.InputManager::PineGetKeyState", GetKeyState);
    Bindings::Register("Pine.Input.InputManager::PineGetMouseButtonState", GetMouseButtonState);
    Bindings::Register("Pine.Input.InputManager::PineGetMousePosition", GetMousePosition);
    Bindings::Register("Pine.Input.InputManager::PineFindInputBinding", LookupInputBind);
    Bindings::Register("Pine.Input.InputManager::PineGetMouseDelta", GetMouseDelta);
    Bindings::Register("Pine.Input.InputManager::PineSetCursorMode", SetCursorMode);
    Bindings::Register("Pine.Input.InputManager::PineCreateInputBinding", CreateInputBinding);

    // InputBind
    Bindings::Register("Pine.Input.InputBind::PineGetInputBindType", GetInputBindType);
    Bindings::Register("Pine.Input.InputBind::PineGetInputBindName", GetInputBindName);
    Bindings::Register("Pine.Input.InputBind::PineGetInputBindAxisValue", GetInputBindAxisValue);
    Bindings::Register("Pine.Input.InputBind::PinePollInputBindActionState", PollInputBindActionState);
    Bindings::Register("Pine.Input.InputBind::PineAddKeyboardBinding", AddKeyboardBinding);
    Bindings::Register("Pine.Input.InputBind::PineAddAxisBinding", AddAxisBinding);
}
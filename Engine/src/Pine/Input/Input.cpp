#include "Input.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"

#include <string.h>
#include <array>
#include <utility>
#include <vector>
#include <GLFW/glfw3.h>

namespace 
{

    Pine::CursorMode m_CursorMode = Pine::CursorMode::Normal;

    std::array<int, GLFW_KEY_LAST> m_KeyStates = {};
    std::array<int, GLFW_KEY_LAST> m_PreviousKeyStates = {};

    std::array<int, GLFW_MOUSE_BUTTON_LAST> m_MouseButtonStates = {};
    std::array<int, GLFW_MOUSE_BUTTON_LAST> m_PreviousMouseButtonStates = {};

    Pine::Vector2i m_MousePosition;
    Pine::Vector2d m_LastMousePosition;
    Pine::Vector2i m_MouseDelta;

    Pine::InputContext* m_Context = nullptr;
    std::vector<Pine::InputContext*> m_InputContexts;

    GLFWwindow* m_Window = nullptr;

}

Pine::InputBind::InputBind(std::string name, InputType type)
    : m_Name(std::move(name)),
      m_Type(type)
{
}

void Pine::InputBind::SetName(const std::string &name)
{
    m_Name = name;
}

const std::string &Pine::InputBind::GetName() const
{
    return m_Name;
}

void Pine::InputBind::SetType(Pine::InputType type)
{
    m_Type = type;
}

Pine::InputType Pine::InputBind::GetType() const
{
    return m_Type;
}

void Pine::InputBind::AddActionCallback(const std::function<void(InputBind * )>& func)
{
    m_ActionCallbacks.push_back(func);
}

bool Pine::InputBind::PollActionState()
{
    bool ret = m_ActionState;

    m_ActionState = false;

    return ret;
}

float Pine::InputBind::GetAxisValue() const
{
    return m_AxisValue;
}

void Pine::InputBind::AddAxisBinding(Pine::Axis axis, float sensitivity)
{
    m_AxisBindings.push_back(AxisBinding{axis, sensitivity});
}

void Pine::InputBind::AddKeyboardBinding(int key, float value)
{
    m_KeyboardBindings.push_back(KeyboardBinding{key, value});
}

void Pine::InputBind::Update()
{
    if (m_Type == InputType::Axis)
    {
        m_AxisValue = 0.f;

        for (auto& axis : m_AxisBindings)
        {
            switch (axis.InputAxis)
            {
            case Axis::MouseX:
                m_AxisValue = m_MouseDelta.x * axis.Sensitivity;
                break;
            case Axis::MouseY:
                m_AxisValue = m_MouseDelta.y * axis.Sensitivity;
                break;
            default:
                break;
            }
        }

        for (auto& key : m_KeyboardBindings)
        {
            if (m_KeyStates[key.Key] == GLFW_PRESS)
            {
                m_AxisValue += key.Value;
            }
        }
    }
    else 
    {
        for (auto& key : m_KeyboardBindings)
        {
            if (m_KeyStates[key.Key] == GLFW_PRESS &&
                m_PreviousKeyStates[key.Key] == GLFW_RELEASE)
            {
                m_ActionState = true;
                
                for (const auto& callback : m_ActionCallbacks)
                {
                    callback(this);
                }
            }
        }
    }
}

void Pine::Input::Setup()
{
    m_InputContexts.push_back(new Pine::InputContext("Default"));
    m_Context = m_InputContexts[0];
}

void Pine::Input::Shutdown()
{
}

void Pine::Input::Update()
{
    auto window = static_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer());
    m_Window = window;

    // Update keys
    memcpy(m_PreviousKeyStates.data(), m_KeyStates.data(), sizeof(m_KeyStates));
    for (int i = 0; i < GLFW_KEY_LAST; i++)
        m_KeyStates[i] = glfwGetKey(window, i);

    // Update mouse positions
    memcpy(m_PreviousMouseButtonStates.data(), m_MouseButtonStates.data(), sizeof(m_MouseButtonStates));
    for (int i = 0; i < GLFW_MOUSE_BUTTON_LAST; i++)
        m_MouseButtonStates[i] = glfwGetMouseButton(window, i);

    // Update mouse position
    Vector2d mousePosition;
    glfwGetCursorPos(window, &mousePosition.x, &mousePosition.y);

    const Vector2d mouseDelta = m_LastMousePosition - mousePosition;
    m_LastMousePosition = mousePosition;
    m_MousePosition = mousePosition;
    m_MouseDelta = mouseDelta;

    for (auto& context : m_InputContexts)
    {
        if (!context->InputEnabled)
            continue;

        for (auto& binding : context->InputBindings)
        {
            binding->Update();
        }
    }

    switch (m_CursorMode)
    {
    case CursorMode::Normal:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;
    case CursorMode::Hidden:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        break;
    case CursorMode::Disabled:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
    }
}

Pine::InputContext* Pine::Input::GetDefaultContext()
{
    return m_InputContexts[0];
}

void Pine::Input::OverrideContext(Pine::InputContext* context)
{
    m_Context = context;
}

Pine::InputBind* Pine::Input::CreateInputBind(const std::string& name, Pine::InputType type)
{
    m_Context->InputBindings.push_back(new InputBind(name, type));

    return m_Context->InputBindings[m_Context->InputBindings.size() - 1];
}

Pine::InputContext* Pine::Input::CreateContext(const std::string& name)
{
    m_InputContexts.push_back(new Pine::InputContext(name));

    return m_InputContexts[m_InputContexts.size() - 1];
}

void Pine::Input::SetCursorPosition(Pine::Vector2i position)
{
    auto windowHandle = static_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer());

    glfwSetCursorPos(windowHandle, position.x, position.y);

    m_LastMousePosition = position;
}

Pine::Vector2i Pine::Input::GetCursorPosition()
{
    auto windowHandle = static_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer());

    double cursorX, cursorY;
    glfwGetCursorPos(windowHandle, &cursorX, &cursorY);

    return { cursorX, cursorY };
}

Pine::Vector2i Pine::Input::GetMouseDelta()
{
    return m_MouseDelta;
}

Pine::KeyState Pine::Input::GetKeyState(Pine::KeyCode key)
{
    const auto currentKeyState = m_KeyStates[static_cast<int>(key)];
    const auto previousKeyState = m_PreviousKeyStates[static_cast<int>(key)];

    if (currentKeyState == GLFW_PRESS)
    {
        return previousKeyState == GLFW_RELEASE ? Pine::KeyState::Pressed : Pine::KeyState::Held;
    }
    else
    {
        return previousKeyState == GLFW_PRESS ? Pine::KeyState::Released : Pine::KeyState::None;
    }
}

Pine::KeyState Pine::Input::GetMouseButtonState(Pine::MouseButton button)
{
    const auto currentKeyState = m_MouseButtonStates[static_cast<int>(button)];
    const auto previousKeyState = m_PreviousMouseButtonStates[static_cast<int>(button)];

    if (currentKeyState == GLFW_PRESS)
    {
        return previousKeyState == GLFW_RELEASE ? Pine::KeyState::Pressed : Pine::KeyState::Held;
    }
    else
    {
        return previousKeyState == GLFW_PRESS ? Pine::KeyState::Released : Pine::KeyState::None;
    }
}

bool Pine::Input::IsKeyDown(Pine::KeyCode key)
{
    return m_KeyStates[static_cast<int>(key)] == GLFW_PRESS;
}

bool Pine::Input::IsMouseButtonDown(Pine::KeyCode code)
{
    return m_MouseButtonStates[static_cast<int>(code)] == GLFW_PRESS;
}

void Pine::Input::SetCursorMode(Pine::CursorMode mode)
{
    m_CursorMode = mode;
}

Pine::InputContext::InputContext(const std::string& name)
{
    Name = name;
}

Pine::InputBind* Pine::InputContext::CreateInputBinding(const std::string& name, Pine::InputType type)
{
    InputBindings.push_back(new InputBind(name, type));
    return InputBindings[InputBindings.size() - 1];
}
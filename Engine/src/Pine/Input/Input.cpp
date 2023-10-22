#include "Input.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"

#include <string.h>
#include <utility>
#include <vector>
#include <GLFW/glfw3.h>

namespace 
{

    bool m_AutoCenterCursor = false;
    bool m_CursorVisible = true;

    int m_KeyStates[GLFW_KEY_LAST] = { };
    int m_KeyStatesOld[GLFW_KEY_LAST] = { };

    Pine::Vector2d m_LastMousePosition;

    Pine::Vector2i m_MousePosition;
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
                m_KeyStatesOld[key.Key] == GLFW_RELEASE)
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
    m_InputContexts.push_back(new Pine::InputContext());
    m_Context = m_InputContexts[0];
}

void Pine::Input::Shutdown()
{
}

void Pine::Input::Update()
{
    auto window = reinterpret_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer());

    m_Window = window;

    // Update keys
    memcpy(m_KeyStatesOld, m_KeyStates, sizeof(m_KeyStates));
    
    for (int i = 0; i < GLFW_KEY_LAST; i++)
        m_KeyStates[i] = glfwGetKey(window, i);

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

    if (m_AutoCenterCursor)
    {
        const auto windowSize = Pine::WindowManager::GetWindowSize();

        glfwSetCursorPos(window, windowSize.x / 2.0, windowSize.y / 2.0);
    
        m_LastMousePosition = Pine::Vector2i(windowSize.x / 2.0, windowSize.y / 2.0);
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

void Pine::Input::SetCursorAutoCenter(bool value)
{
    m_AutoCenterCursor = value;
}

void Pine::Input::SetCursorVisible(bool value)
{
    m_CursorVisible = value;
}

Pine::InputBind* Pine::Input::CreateInputBind(const std::string& name, Pine::InputType type)
{
    m_Context->InputBindings.push_back(new InputBind(name, type));

    return m_Context->InputBindings[m_Context->InputBindings.size() - 1];
}

Pine::InputContext* Pine::Input::CreateContext()
{
    m_InputContexts.push_back(new Pine::InputContext());

    return m_InputContexts[m_InputContexts.size() - 1];
}
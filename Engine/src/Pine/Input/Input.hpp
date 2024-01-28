#pragma once

#include <string>
#include <vector>
#include <functional>

#include "Pine/Core/Math/Math.hpp"

namespace Pine
{

    enum class InputType
    {
        Axis,
        Action
    };

    enum class Axis
    {
        None,
        MouseX,
        MouseY
    };

    struct AxisBinding
    {
        Axis InputAxis = Axis::None;
        float Sensitivity = 1.f;
    };

    struct KeyboardBinding
    {
        int Key = 0;

        // Used whenever this is bound to an axis input binding
        float Value = 1.f;
    };

    class InputBind
    {
    private:
        std::string m_Name;
        InputType m_Type = InputType::Axis;

        float m_AxisValue = 0.0f;

        bool m_ActionState = false;
        std::vector<std::function<void(InputBind*)>> m_ActionCallbacks;

        std::vector<AxisBinding> m_AxisBindings;
        std::vector<KeyboardBinding> m_KeyboardBindings;
    public:
        explicit InputBind(std::string name, InputType type = InputType::Axis);

        void SetName(const std::string& name);
        const std::string& GetName() const;

        void SetType(InputType type);
        InputType GetType() const;

        void AddAxisBinding(Axis axis, float sensitivity = 1.f);
        void AddKeyboardBinding(int key, float value = 1.f);
        void AddActionCallback(const std::function<void(InputBind*)>& func);

        bool PollActionState();
        float GetAxisValue() const;

        void Update();
    };

    struct InputContext
    {  
        std::string Name;
        bool InputEnabled = true;
        std::vector<InputBind*> InputBindings;

        explicit InputContext(const std::string& name);
        InputBind* CreateInputBinding(const std::string& name, InputType type = InputType::Axis);
    };

    namespace Input
    {
        void Setup();
        void Shutdown();
        void Update();

        InputContext* CreateContext(const std::string& name);
        InputContext* GetDefaultContext();

        void OverrideContext(InputContext* context);

        void SetCursorAutoCenter(bool value);
        void SetCursorVisible(bool value);

        void SetCursorPosition(Pine::Vector2i position);
        Pine::Vector2i GetCursorPosition();

        InputBind* CreateInputBind(const std::string& name, InputType type = InputType::Axis);
    }

}
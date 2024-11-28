#pragma once

#include <string>
#include <vector>
#include <functional>

#include "Pine/Core/Math/Math.hpp"

namespace Pine
{

    enum class KeyCode
    {
        Space = 32,
        Apostrophe = 39,
        Comma = 44,
        Minus = 45,
        Period = 46,
        Slash = 47,
        Num0 = 48,
        Num1 = 49,
        Num2 = 50,
        Num3 = 51,
        Num4 = 52,
        Num5 = 53,
        Num6 = 54,
        Num7 = 55,
        Num8 = 56,
        Num9 = 57,
        Semicolon = 59,
        Equal = 61,
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        Escape = 256,
        Enter = 257,
        Tab = 258,
        Backspace = 259,
        Insert = 260,
        Delete = 261,
        Right = 262,
        Left = 263,
        Down = 264,
        Up = 265,
        PageUp = 266,
        PageDown = 267,
        Home = 268,
        End = 269,
        CapsLock = 280,
        ScrollLock = 281,
        NumLock = 282,
        PrintScreen = 283,
        Pause = 284,
        F1 = 290,
        F2 = 291,
        F3 = 292,
        F4 = 293,
        F5 = 294,
        F6 = 295,
        F7 = 296,
        F8 = 297,
        F9 = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,
        F13 = 302,
        F14 = 303,
        F15 = 304,
        F16 = 305,
        F17 = 306,
        F18 = 307,
        F19 = 308,
        F20 = 309,
        F21 = 310,
        F22 = 311,
        F23 = 312,
        F24 = 313,
        F25 = 314,
        NumPad0 = 320,
        NumPad1 = 321,
        NumPad2 = 322,
        NumPad3 = 323,
        NumPad4 = 324,
        NumPad5 = 325,
        NumPad6 = 326,
        NumPad7 = 327,
        NumPad8 = 328,
        NumPad9 = 329,
        NumPadDecimal = 330,
        NumPadDivide = 331,
        NumPadMultiply = 332,
        NumPadSubtract = 333,
        NumPadAdd = 334,
        NumPadEnter = 335,
        NumPadEqual = 336,
        LeftShift = 340,
        LeftControl = 341,
        LeftAlt = 342,
        LeftSuper = 343,
        RightShift = 344,
        RightControl = 345,
        RightAlt = 346,
        RightSuper = 347,
        Menu = 348
    };

    enum class KeyState
    {
        None, // Key is not being pressed
        Pressed, // Key was just pressed
        Held, // Key is being held
        Released // Key was just released
    };

    enum class MouseButton
    {
        Left = 0,
        Right,
        Middle
    };

    enum class InputType
    {
        Axis,
        Action
    };

    enum class CursorMode
    {
        Normal,
        Hidden,
        Disabled
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

        void SetCursorMode(CursorMode mode);

        void SetCursorPosition(Pine::Vector2i position);
        Pine::Vector2i GetCursorPosition();

        InputBind* CreateInputBind(const std::string& name, InputType type = InputType::Axis);
        InputBind* FindInputBind(const std::string& name);

        KeyState GetKeyState(KeyCode key);
        KeyState GetMouseButtonState(MouseButton button);

        bool IsKeyDown(KeyCode key);
        bool IsMouseButtonDown(KeyCode code);
    }

}
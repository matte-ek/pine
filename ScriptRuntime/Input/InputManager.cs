using Pine.Core;
using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.Input
{
    public enum KeyState
    {
        None, // Key is not being pressed
        Pressed, // Key was just pressed
        Held, // Key is being held
        Released // Key was just released
    }
    
    public enum MouseButton
    {
        Left,
        Right,
        Middle
    }

    public enum CursorMode
    {
        Normal,   // Visible, free cursor
        Hidden,   // Hidden, but not locked
        Disabled  // Hidden and locked to the window - use this for mouse-look
    }
    
    public unsafe class InputManager
    {
        public static Vector2 MousePosition
        {
            get
            {
                Vector2 position;

                InputBindings.GetMousePosition(&position);

                return position;
            }
        }

        public static Vector2 GetMouseDelta
        {
            get
            {
                Vector2 delta;

                InputBindings.GetMouseDelta(&delta);

                return delta;
            }
        }

        public static void SetCursorMode(CursorMode mode) => InputBindings.SetCursorMode((int)mode);

        public static bool IsKeyDown(KeyCode key) => InputBindings.IsKeyDown((int)key) != 0;
        public static bool IsMouseButtonDown(MouseButton mouseButton) => InputBindings.IsMouseButtonDown((int)mouseButton) != 0;

        public static KeyState GetKeyState(KeyCode key) => (KeyState)InputBindings.GetKeyState((int)key);
        public static KeyState GetMouseButtonState(MouseButton mouseButton) => (KeyState)InputBindings.GetMouseButtonState((int)mouseButton);

        public static InputBind CreateInputBind(string name, InputBindType type = InputBindType.Axis)
        {
            using var text = new Interop.Utf8Scope(name);

            return new InputBind(InputBindings.CreateInputBinding(text.Pointer, (int)type));
        }

        public static InputBind FindInput(string name)
        {
            using var text = new Interop.Utf8Scope(name);

            return new InputBind(InputBindings.FindInputBinding(text.Pointer));
        }
    }
}

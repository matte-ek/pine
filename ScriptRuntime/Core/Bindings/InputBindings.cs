using System;
using Pine.Math;

namespace Pine.Core.Bindings
{
    // The engine functions behind Pine.Input.
    internal static unsafe class InputBindings
    {
        public static delegate* unmanaged<int, byte> IsKeyDown;
        public static delegate* unmanaged<int, byte> IsMouseButtonDown;
        public static delegate* unmanaged<int, int> GetKeyState;
        public static delegate* unmanaged<int, int> GetMouseButtonState;
        public static delegate* unmanaged<Vector2*, void> GetMousePosition;
        public static delegate* unmanaged<Vector2*, void> GetMouseDelta;
        public static delegate* unmanaged<int, void> SetCursorMode;
        public static delegate* unmanaged<byte*, int, int> CreateInputBinding;
        public static delegate* unmanaged<byte*, int> FindInputBinding;

        public static delegate* unmanaged<int, int> GetBindType;
        public static delegate* unmanaged<int, IntPtr> GetBindName;
        public static delegate* unmanaged<int, float> GetBindAxisValue;
        public static delegate* unmanaged<int, byte> PollBindActionState;
        public static delegate* unmanaged<int, int, float, void> AddKeyboardBinding;
        public static delegate* unmanaged<int, int, float, void> AddAxisBinding;

        public static void Bind()
        {
            IsKeyDown = (delegate* unmanaged<int, byte>)Interop.Resolve("Pine.Input.InputManager::PineIsKeyDown");
            IsMouseButtonDown = (delegate* unmanaged<int, byte>)Interop.Resolve("Pine.Input.InputManager::PineIsMouseButtonDown");
            GetKeyState = (delegate* unmanaged<int, int>)Interop.Resolve("Pine.Input.InputManager::PineGetKeyState");
            GetMouseButtonState = (delegate* unmanaged<int, int>)Interop.Resolve("Pine.Input.InputManager::PineGetMouseButtonState");
            GetMousePosition = (delegate* unmanaged<Vector2*, void>)Interop.Resolve("Pine.Input.InputManager::PineGetMousePosition");
            GetMouseDelta = (delegate* unmanaged<Vector2*, void>)Interop.Resolve("Pine.Input.InputManager::PineGetMouseDelta");
            SetCursorMode = (delegate* unmanaged<int, void>)Interop.Resolve("Pine.Input.InputManager::PineSetCursorMode");
            CreateInputBinding = (delegate* unmanaged<byte*, int, int>)Interop.Resolve("Pine.Input.InputManager::PineCreateInputBinding");
            FindInputBinding = (delegate* unmanaged<byte*, int>)Interop.Resolve("Pine.Input.InputManager::PineFindInputBinding");

            GetBindType = (delegate* unmanaged<int, int>)Interop.Resolve("Pine.Input.InputBind::PineGetInputBindType");
            GetBindName = (delegate* unmanaged<int, IntPtr>)Interop.Resolve("Pine.Input.InputBind::PineGetInputBindName");
            GetBindAxisValue = (delegate* unmanaged<int, float>)Interop.Resolve("Pine.Input.InputBind::PineGetInputBindAxisValue");
            PollBindActionState = (delegate* unmanaged<int, byte>)Interop.Resolve("Pine.Input.InputBind::PinePollInputBindActionState");
            AddKeyboardBinding = (delegate* unmanaged<int, int, float, void>)Interop.Resolve("Pine.Input.InputBind::PineAddKeyboardBinding");
            AddAxisBinding = (delegate* unmanaged<int, int, float, void>)Interop.Resolve("Pine.Input.InputBind::PineAddAxisBinding");
        }
    }
}

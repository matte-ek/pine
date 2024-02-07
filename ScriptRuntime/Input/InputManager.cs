using System.Runtime.CompilerServices;
using Pine.Assets;
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
    
    public class InputManager
    {
        public static Vector2 MousePosition
        {
            get
            {
                PineGetMousePosition(out var position);
                return position;
            }
        }
        
        public static KeyState GetKeyState(KeyCode key) => (KeyState)PineGetKeyState((int)key);
        public static KeyState GetMouseButtonState(MouseButton mouseButton) => (KeyState)PineGetMouseButtonState((int)mouseButton);
        
        public static InputBind FindInputBind(string name) => PineFindInputBinding(name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetKeyState(int key);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetMouseButtonState(int mouseButton);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern InputBind PineFindInputBinding(string name);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineGetMousePosition(out Vector2 position);
    }
}
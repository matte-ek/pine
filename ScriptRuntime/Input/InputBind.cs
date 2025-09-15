using System.Runtime.CompilerServices;

namespace Pine.Input
{
    public enum InputBindType
    {
        Axis,
        Action
    }

    public enum Axis
    {
        None,
        MouseX,
        MouseY
    }

    public class InputBind
    {
        public string Name => PineGetInputBindName(_id);
        public InputBindType Type => (InputBindType)PineGetInputBindType(_id);
        
        public float Value => PineGetInputBindAxisValue(_id);
        public bool ActionState => PinePollInputBindActionState(_id);
        
        public void AddKeyboardBinding(KeyCode key, float value = 1.0f) => PineAddKeyboardBinding(_id, (int)key, value);
        public void AddAxisBinding(Axis axis, float sensitivity = 1.0f) => PineAddAxisBinding(_id, (int)axis, sensitivity);
        
        private readonly int _id;
        
        internal InputBind(int id)
        {
            _id = id;
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetInputBindType(int id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string PineGetInputBindName(int id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetInputBindAxisValue(int id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PinePollInputBindActionState(int id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineAddKeyboardBinding(int id, int key, float value = 1.0f);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineAddAxisBinding(int id, int axis, float sensitivity = 1.0f);
    }
}
using System.Runtime.CompilerServices;

namespace Pine.Input
{
    public enum InputBindType
    {
        Axis,
        Action
    }
    
    public class InputBind
    {
        internal InputBind(int id)
        {
            _id = id;
        }

        private int _id = -1;
        
        public readonly string Name;
        public readonly InputBindType Type;
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetKeyState(int key);
    }
}
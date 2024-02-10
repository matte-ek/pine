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
        public string Name => PineGetInputBindName(_id);
        public InputBindType Type => (InputBindType)PineGetInputBindType(_id);
        
        public float Value => PineGetInputBindValue(_id);
        public bool ActionState => PinePollInputBindActionState(_id);
        
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
        private static extern float PineGetInputBindValue(int id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PinePollInputBindActionState(int id);
    }
}
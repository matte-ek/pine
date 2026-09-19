using Pine.Core;
using Pine.Core.Bindings;

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

    public unsafe class InputBind
    {
        public string Name => Interop.StringFrom(InputBindings.GetBindName(_id));
        public InputBindType Type => (InputBindType)InputBindings.GetBindType(_id);

        public float Value => InputBindings.GetBindAxisValue(_id);
        public bool ActionState => InputBindings.PollBindActionState(_id) != 0;

        public void AddKeyboardBinding(KeyCode key, float value = 1.0f)
            => InputBindings.AddKeyboardBinding(_id, (int)key, value);

        public void AddAxisBinding(Axis axis, float sensitivity = 1.0f)
            => InputBindings.AddAxisBinding(_id, (int)axis, sensitivity);

        private readonly int _id;

        internal InputBind(int id)
        {
            _id = id;
        }
    }
}

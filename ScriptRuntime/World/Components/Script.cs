using Pine.Assets;
using Pine.Core;
using Pine.Core.Bindings;

namespace Pine.World.Components
{
    [ComponentType(ComponentType.Script)]
    public unsafe class Script : Component
    {
        public CSharpScript ScriptAsset
            => Interop.ObjectFrom<CSharpScript>(ComponentBindings.ScriptGetCSharpScript(InternalId));

        public T GetScriptInstance<T>() where T : Script => (T)GetScriptInstance();

        public Script GetScriptInstance()
            => Interop.ObjectFrom<Script>(ComponentBindings.ScriptGetInstance(InternalId));

        protected Transform Transform => Parent.Transform;
    }
}

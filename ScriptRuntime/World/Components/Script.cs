using System.Runtime.CompilerServices;
using Pine.Assets;

namespace Pine.World.Components
{
    public class Script : Component
    {
        public CSharpScript ScriptAsset => (CSharpScript)GetScript(InternalId);

        public T GetScriptInstance<T>() where T : Script => (T)GetScriptInstanceInternal(InternalId);
        public Script GetScriptInstance() => GetScriptInstanceInternal(InternalId);
        
        protected Transform Transform => Parent.Transform;
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Asset GetScript(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Script GetScriptInstanceInternal(uint id);
    }
}
using Pine.World;
using Pine.World.Components;

namespace Pine.Core
{
    public class Script : Component
    {
        protected Transform Transform => Parent.Transform;
    }
}
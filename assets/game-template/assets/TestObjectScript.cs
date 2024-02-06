using Pine.Core;
using Pine.Math;

namespace Game
{
    public class TestObjectScript : Script
    {
        public void OnStart()
        {
            Log.Info($"Entity Name: {Parent.Name}");
            Log.Info($"Current position, X: {Transform.Position.X}, Y: {Transform.Position.Y}, Z: {Transform.Position.Z}");
        }
        
        public void OnUpdate(float deltaTime)
        {
        }
    }
}
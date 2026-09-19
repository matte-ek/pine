using System.Runtime.InteropServices;

namespace Pine.Math
{
    // Blittable, and required to stay that way: it crosses to the engine as raw bytes, by
    // pointer, through function pointers that accept nothing else. Stating the layout records
    // that where someone might otherwise add a field that is not.
    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion
    {
        public float X { get; set; }
        public float Y { get; set; }
        public float Z { get; set; }
        public float W { get; set; }
    
        public Quaternion(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }
    
        public Quaternion(float value)
        {
            X = value;
            Y = value;
            Z = value;
            W = value;
        }
    }
}
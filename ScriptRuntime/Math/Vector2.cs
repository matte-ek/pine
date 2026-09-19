using System.Runtime.InteropServices;

namespace Pine.Math
{
    // Blittable, and required to stay that way: it crosses to the engine as raw bytes, by value
    // and by pointer, through function pointers that accept nothing else. Stating the layout
    // records that where someone might otherwise add a field that is not.
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float X { get; set; }
        public float Y { get; set; }
    
        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }
    
        public Vector2(float value)
        {
            X = value;
            Y = value;
        }
    
        public static Vector2 operator +(Vector2 a, Vector2 b)
        {
            return new Vector2(a.X + b.X, a.Y + b.Y);
        }
    
        public static Vector2 operator -(Vector2 a, Vector2 b)
        {
            return new Vector2(a.X - b.X, a.Y - b.Y);
        }
    
        public static Vector2 operator *(Vector2 a, Vector2 b)
        {
            return new Vector2(a.X * b.X, a.Y * b.Y);
        }
    
        public static Vector2 operator /(Vector2 a, Vector2 b)
        {
            return new Vector2(a.X / b.X, a.Y / b.Y);
        }
    }
}
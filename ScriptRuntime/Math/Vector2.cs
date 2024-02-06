namespace Pine.Math
{
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
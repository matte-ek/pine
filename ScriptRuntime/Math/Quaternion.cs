namespace Pine.Math
{
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
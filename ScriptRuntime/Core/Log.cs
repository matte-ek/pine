using System.Runtime.CompilerServices;

namespace Pine.Core
{
    public class Log
    {
        public static void Info(string message)
        {
            PineInfo(message);
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineVerbose(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineInfo(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineWarning(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineError(string message);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineFatal(string message);
    }
}
using System.Runtime.CompilerServices;

namespace Pine.Core
{
    public class Log
    {
        public static void Info(string message)
        {
            PineInfo(message);
        }
        
        public static void Verbose(string message)
        {
            PineVerbose(message);
        }
        
        public static void Warning(string message)
        {
            PineWarning(message);
        }
        
        public static void Error(string message)
        {
            PineError(message);
        }
        
        public static void Fatal(string message)
        {
            PineFatal(message);
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
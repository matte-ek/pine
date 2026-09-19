namespace Pine.Core.Bindings
{
    // The engine functions behind Pine.Core.Log.
    internal static unsafe class LogBindings
    {
        public static delegate* unmanaged<byte*, void> Verbose;
        public static delegate* unmanaged<byte*, void> Info;
        public static delegate* unmanaged<byte*, void> Warning;
        public static delegate* unmanaged<byte*, void> Error;
        public static delegate* unmanaged<byte*, void> Fatal;

        public static void Bind()
        {
            Verbose = (delegate* unmanaged<byte*, void>)Interop.Resolve("Pine.Core.Log::PineVerbose");
            Info = (delegate* unmanaged<byte*, void>)Interop.Resolve("Pine.Core.Log::PineInfo");
            Warning = (delegate* unmanaged<byte*, void>)Interop.Resolve("Pine.Core.Log::PineWarning");
            Error = (delegate* unmanaged<byte*, void>)Interop.Resolve("Pine.Core.Log::PineError");
            Fatal = (delegate* unmanaged<byte*, void>)Interop.Resolve("Pine.Core.Log::PineFatal");
        }
    }
}

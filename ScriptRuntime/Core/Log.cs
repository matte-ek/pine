using Pine.Core.Bindings;

namespace Pine.Core
{
    public static unsafe class Log
    {
        public static void Info(string message)
        {
            Write(LogBindings.Info, message);
        }

        public static void Verbose(string message)
        {
            Write(LogBindings.Verbose, message);
        }

        public static void Warning(string message)
        {
            Write(LogBindings.Warning, message);
        }

        public static void Error(string message)
        {
            Write(LogBindings.Error, message);
        }

        public static void Fatal(string message)
        {
            Write(LogBindings.Fatal, message);
        }

        private static unsafe void Write(delegate* unmanaged<byte*, void> binding, string message)
        {
            // Logging is the first thing bound, and what everything else reports its own failures
            // through. A null binding therefore means binding itself did not get that far - and
            // calling through it would turn a reportable failure into a crash.
            if (binding == null)
            {
                return;
            }

            using var text = new Interop.Utf8Scope(message);

            binding(text.Pointer);
        }
    }
}

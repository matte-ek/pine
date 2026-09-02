using System;
using System.Runtime.InteropServices;

namespace Pine.Core
{
    // A stable, unique identifier for engine objects (e.g. entities). This is the
    // managed mirror of the native Pine::UId — two 64-bit halves (a nanosecond
    // timestamp + a random value). The layout must stay in sync with
    // Engine/src/Pine/Core/UId/UId.hpp, as the native side sets this by raw value.
    [StructLayout(LayoutKind.Sequential)]
    public readonly struct UId : IEquatable<UId>
    {
        public readonly ulong Time;
        public readonly ulong Random;

        public bool IsValid => Time != 0;

        public bool Equals(UId other) => Time == other.Time && Random == other.Random;
        public override bool Equals(object obj) => obj is UId other && Equals(other);
        public override int GetHashCode() => (Time ^ Random).GetHashCode();

        public static bool operator ==(UId a, UId b) => a.Equals(b);
        public static bool operator !=(UId a, UId b) => !a.Equals(b);

        public override string ToString() => $"{Time:x}-{Random:x16}";
    }
}

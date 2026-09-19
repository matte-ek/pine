#pragma once

#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Pine::Script
{
    // The presentation choices that are a yes or no rather than a value.
    // Mirrors Pine.Core.Reflection.ScriptFieldFlags.
    enum ScriptFieldFlags : std::uint32_t
    {
        ScriptFieldFlag_None = 0,
        ScriptFieldFlag_HasRange = 1 << 0,
        ScriptFieldFlag_HasSpace = 1 << 1
    };

    // One reflected field, as Pine.dll hands it over. Member for member and in order, this is
    // Pine.Core.Reflection.ScriptFieldDescriptor; FieldRegistry::Setup checks the two agree on
    // size, so a member added to one side alone is caught rather than silently misread.
    //
    // The three strings are UTF-8 and owned by the managed registry. They stay valid until it is
    // reset, which happens on every reload - copy out of them, never hold one.
    struct ScriptFieldDescriptor
    {
        const char* Name = nullptr;
        std::int32_t Type = 0;
        std::int32_t AssetType = 0;
        std::uint32_t Flags = 0;
        float RangeMin = 0.f;
        float RangeMax = 0.f;
        const char* Header = nullptr;
        const char* Tooltip = nullptr;
    };

    // The engine's half of Pine.Core.Reflection.FieldRegistry, which decides which of a script's
    // fields the editor shows and does the reading and writing of their values.
    //
    // The engine addresses a reflected class by the id Register hands back, and a field by its
    // position within that class. Both are good until the next Reset.
    //
    // Include it from a .cpp under Script/ and nowhere else, like the rest of the hosting half.
    namespace FieldRegistry
    {
        // Resolve the managed entry points, and check that the engine and Pine.dll still agree on
        // the shape of a descriptor. Called once, when the runtime comes up: the registry lives in
        // Pine.dll, which a game-assembly reload no longer takes with it.
        bool Setup();

        // Drop every registration, and with it the strings the descriptors point at.
        void Reset();

        // Reflect one script class, returning the id its fields are addressed by, or -1. The
        // class crosses as a handle to its managed Type - see GameAssembly::ScopedClassType.
        int Register(std::uint64_t classTypeHandle);

        // Every reflected field of a registered class, in the order they should be shown. Empty
        // when the class has no reflectable fields, and when it could not be read at all.
        std::vector<ScriptFieldDescriptor> GetDescriptors(int classId);

        // One field's value in the form the engine stores it. ReadValue sizes data itself, since
        // a string's length is not known until it is asked for.
        bool ReadValue(const ObjectHandle& object, int classId, int fieldIndex, std::vector<std::byte>& data);
        bool WriteValue(const ObjectHandle& object, int classId, int fieldIndex, const std::vector<std::byte>& data);
    }
}

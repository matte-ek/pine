#pragma once

#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace Pine::Script
{
    // One of the game's script classes, as the game assembly's registry hands it over.
    struct ResolvedScriptClass
    {
        // Where the registry keeps the class, or -1 when there is no such class or it does not
        // derive from Pine.World.Components.Script. Good until the next reload.
        int Id = -1;

        // Which lifecycle methods it has. All of them are optional, so the engine only dispatches
        // the ones a class actually declares - or inherits from a script it derives from.
        bool HasOnStart = false;
        bool HasOnUpdate = false;
    };

    // The game's own managed assembly.
    //
    // It lives in a load context of its own, separate from the engine's Pine.dll, so that
    // rebuilding a script can replace it while the editor is running. That is the whole reason
    // for this seam: the engine never holds a managed type or method from the game assembly
    // itself - only the ids below - because anything it held would keep the old assembly loaded
    // and quietly turn every reload into a leak.
    //
    // This names the hosting API through ScriptObjectFactory, so include it from a .cpp under
    // Script/ and nowhere else.
    namespace GameAssembly
    {
        // Resolve the managed entry points. Called once, when the runtime comes up.
        void Setup();

        // Replace whatever was loaded before. Unload() first - loading does not do it for you,
        // because the engine has its own state to take down in between.
        bool Load(const std::filesystem::path& path);
        void Unload();

        ResolvedScriptClass ResolveClass(const std::string& namespaceName, const std::string& className);

        // A handle to a script class' managed Type, for the two entry points that are handed a
        // class to work with rather than an id. It lives no longer than the call it is made for -
        // managed code that needs the type for longer keeps its own reference to it - so it is
        // scoped rather than handed out raw.
        class ScopedClassType
        {
        public:
            explicit ScopedClassType(int classId);
            ~ScopedClassType();

            ScopedClassType(const ScopedClassType&) = delete;
            ScopedClassType& operator=(const ScopedClassType&) = delete;

            std::uint64_t GetHandle() const;
            bool IsValid() const;

        private:
            std::uint64_t m_Handle = 0;
        };

        void OnStart(const ObjectHandle& script, int classId);
        void OnUpdate(const ObjectHandle& script, int classId, float deltaTime);
    }
}

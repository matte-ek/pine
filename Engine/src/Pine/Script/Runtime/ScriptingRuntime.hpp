#pragma once

// The scripting runtime's lifecycle, which is all the rest of the engine needs from it. Hosting
// the runtime, loading assemblies and reaching managed objects is Script/'s business - see
// Script/ManagedCall/ManagedCall.hpp and Script/GameAssembly/GameAssembly.hpp.
namespace Pine::Script::Runtime
{
    // Collect whatever managed objects nothing refers to any more. The editor asks for this after
    // stopping play, where reloading the level has just dropped a scene's worth of mirrors.
    void RunGarbageCollector();

    // Start the runtime and load the engine's own managed assembly. Both happen once: a game
    // assembly is loaded and reloaded on top of this, and never takes the runtime with it.
    //
    // A machine with no .NET runtime installed is not an error the engine stops for - this logs,
    // answers false, and leaves IsAvailable() saying scripting is off.
    bool Setup();

    void Dispose();

    bool IsAvailable();
}

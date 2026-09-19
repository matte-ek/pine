#pragma once

#include <string>

// The engine functions C# is allowed to call.
//
// The scripting runtime gives managed code no way of its own to reach the engine, so every
// binding is an ordinary engine function whose address Pine.dll is handed at boot. The six
// Interfaces::*::Setup tables name them here; Pine.Core.Interop asks for them back by the same
// name and calls them through a function pointer.
//
// Names are resolved rather than positional on purpose: a binding Pine.dll asks for and the
// engine never registered - or the other way about - is a line in the log naming it, at startup,
// instead of a call that quietly lands on the wrong function.
namespace Pine::Script::Bindings
{
    // Register every binding. Called once, before Pine.dll is told how to find them.
    void Setup();

    void Register(const char* name, void* function);

    // The same, taking the function itself. The cast is identical at all 149 call sites and says
    // nothing at any of them.
    template <typename Function>
    void Register(const char* name, Function* function)
    {
        Register(name, reinterpret_cast<void*>(function));
    }

    // The engine function registered under a name, or null with the miss reported. This is what
    // Pine.dll calls to bind itself, and the only thing the engine hands it to do so.
    void* Resolve(const char* name);

    // A string for a binding to return. Managed code never frees what the engine hands it, so a
    // returned string is borrowed rather than given: this copies it into a buffer of the calling
    // thread's own, and the pointer is good until that thread's next call to a binding that
    // returns one. The managed side copies it out immediately - see Interop.StringFrom.
    const char* ReturnString(const std::string& text);
}

#pragma once

#include <coreclr_delegates.h>

// Calling into Pine.dll.
//
// A handful of the engine's scripting jobs - reflecting a script's fields, creating the managed
// mirror of an engine object, dispatching a script's lifecycle methods - are done by managed code
// in Pine.dll rather than by the engine itself, because all of them are reflection. The engine
// reaches that code through a small set of static entry points, resolved once at boot and called
// through an ordinary function pointer from then on.
//
// An entry point is a [UnmanagedCallersOnly] method, so its parameters and return value are
// blittable and nothing may be thrown out of it - every one of them catches, logs and answers
// with a failure value instead. An entry point that failed to resolve stays null, and its caller
// answers with the same failure value rather than calling through nothing.
//
// This names the hosting API, so include it from a .cpp under Script/ and nowhere else.
namespace Pine::Script::ManagedCall
{
    // Hand over the delegate entry points are resolved through, once the runtime is up.
    void Setup(get_function_pointer_fn getFunctionPointer);

    // One static entry point, or null with the miss reported. The type name is
    // assembly-qualified, as the hosting API wants it: "Pine.Core.ObjectFactory, Pine".
    void* FindEntryPoint(const char* typeName, const char* methodName);

    // The same, as the function pointer the caller is going to hold it in.
    template <typename Signature>
    Signature Find(const char* typeName, const char* methodName)
    {
        return reinterpret_cast<Signature>(FindEntryPoint(typeName, methodName));
    }
}

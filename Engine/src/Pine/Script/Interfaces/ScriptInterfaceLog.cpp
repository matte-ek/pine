#include <mono/metadata/appdomain.h>
#include "Pine/Core/Log/Log.hpp"
#include "Interfaces.hpp"

namespace
{

    void PineVerbose(MonoString* string)
    {
        auto str = mono_string_to_utf8(string);
        Pine::Log::Verbose(str);
        mono_free(str);
    }

    void PineInfo(MonoString* string)
    {
        auto str = mono_string_to_utf8(string);
        Pine::Log::Info(str);
        mono_free(str);
    }

    void PineWarning(MonoString* string)
    {
        auto str = mono_string_to_utf8(string);
        Pine::Log::Warning(str);
        mono_free(str);
    }

    void PineError(MonoString* string)
    {
        auto str = mono_string_to_utf8(string);
        Pine::Log::Error(str);
        mono_free(str);
    }

    void PineFatal(MonoString* string)
    {
        auto str = mono_string_to_utf8(string);
        Pine::Log::Fatal(str);
        mono_free(str);
    }

}

void Pine::Script::Interfaces::Log::Setup()
{
    mono_add_internal_call("Pine.Core.Log::PineVerbose", reinterpret_cast<void*>(PineVerbose));
    mono_add_internal_call("Pine.Core.Log::PineInfo", reinterpret_cast<void*>(PineInfo));
    mono_add_internal_call("Pine.Core.Log::PineWarning", reinterpret_cast<void*>(PineWarning));
    mono_add_internal_call("Pine.Core.Log::PineError", reinterpret_cast<void*>(PineError));
    mono_add_internal_call("Pine.Core.Log::PineFatal", reinterpret_cast<void*>(PineFatal));
}

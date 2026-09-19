#include "Interfaces.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"

namespace
{
    void PineVerbose(const char* message)
    {
        PVerbose(message);
    }

    void PineInfo(const char* message)
    {
        PInfo(message);
    }

    void PineWarning(const char* message)
    {
        PWarning(message);
    }

    void PineError(const char* message)
    {
        PError(message);
    }

    void PineFatal(const char* message)
    {
        PFatal(message);
    }
}

void Pine::Script::Interfaces::Log::Setup()
{
    Bindings::Register("Pine.Core.Log::PineVerbose", PineVerbose);
    Bindings::Register("Pine.Core.Log::PineInfo", PineInfo);
    Bindings::Register("Pine.Core.Log::PineWarning", PineWarning);
    Bindings::Register("Pine.Core.Log::PineError", PineError);
    Bindings::Register("Pine.Core.Log::PineFatal", PineFatal);
}

#include "ManagedCall.hpp"

#include "Pine/Core/Log/Log.hpp"

namespace
{
    get_function_pointer_fn m_GetFunctionPointer = nullptr;
}

void Pine::Script::ManagedCall::Setup(const get_function_pointer_fn getFunctionPointer)
{
    m_GetFunctionPointer = getFunctionPointer;
}

void* Pine::Script::ManagedCall::FindEntryPoint(const char* typeName, const char* methodName)
{
    if (m_GetFunctionPointer == nullptr)
    {
        return nullptr;
    }

    void* entryPoint = nullptr;

    // UNMANAGEDCALLERSONLY_METHOD in place of a delegate type: the method carries
    // [UnmanagedCallersOnly] and is called directly, with no managed wrapper in between.
    const auto result = m_GetFunctionPointer(typeName, methodName, UNMANAGEDCALLERSONLY_METHOD,
        nullptr, nullptr, &entryPoint);

    if (result != 0 || entryPoint == nullptr)
    {
        PError(fmt::format("Script: Pine.dll has no {} entry point on {} (0x{:x}).",
            methodName, typeName, static_cast<std::uint32_t>(result)));

        return nullptr;
    }

    return entryPoint;
}

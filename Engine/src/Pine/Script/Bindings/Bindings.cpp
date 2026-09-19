#include "Bindings.hpp"

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Interfaces/Interfaces.hpp"

#include <string>
#include <unordered_map>

namespace
{
    std::unordered_map<std::string, void*> m_Bindings;
}

void Pine::Script::Bindings::Setup()
{
    Interfaces::Log::Setup();
    Interfaces::Entity::Setup();
    Interfaces::Component::Setup();
    Interfaces::Asset::Setup();
    Interfaces::Input::Setup();
    Interfaces::Physics::Setup();
}

void Pine::Script::Bindings::Register(const char* name, void* function)
{
    const auto existing = m_Bindings.find(name);

    // Two engine functions under one name means one of them is unreachable, and which one is
    // whichever table ran last. Worth saying out loud rather than resolving by accident.
    if (existing != m_Bindings.end() && existing->second != function)
    {
        PError(fmt::format("Script: {} is registered as a binding twice.", name));
    }

    m_Bindings[name] = function;
}

void* Pine::Script::Bindings::Resolve(const char* name)
{
    const auto found = m_Bindings.find(name);

    if (found == m_Bindings.end())
    {
        // Pine.dll and the engine are built against different versions of each other. Pine.dll
        // gives up over this rather than carrying on with a binding it cannot call, so this is
        // the only place the name of the offending one is ever said.
        PError(fmt::format("Script: Pine.dll asked for the binding {}, which the engine does not "
                           "register.", name));

        return nullptr;
    }

    return found->second;
}

const char* Pine::Script::Bindings::ReturnString(const std::string& text)
{
    thread_local std::string buffer;

    buffer = text;

    return buffer.c_str();
}

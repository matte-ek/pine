#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Pine/World/Components/Component/Component.hpp"

namespace Editor::DebugServer::Editing::Components
{
    struct Adapter
    {
        Pine::ComponentType Type;
        const char* Name;
        nlohmann::json Properties;

        // A null component asks for creation defaults, without allocating a live component.
        nlohmann::json (*Read)(const Pine::Component* component);
        void (*Validate)(const nlohmann::json& state, const std::string& path);
        void (*Apply)(Pine::Component* component, const nlohmann::json& state);

        // Opt in only after validating creation/removal side effects and dependencies.
        bool AllowAddRemove = false;
    };

    const std::vector<const Adapter*>& GetAdapters();
    const Adapter* Find(const std::string& name);
    const Adapter* Find(Pine::ComponentType type);

    nlohmann::json Prepare(const Adapter& adapter, nlohmann::json state,
        const nlohmann::json& properties, const std::string& path);
    nlohmann::json Describe(const Adapter& adapter, const Pine::Component* component);
}

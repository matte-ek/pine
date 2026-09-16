#pragma once
#include "../Components.hpp"

namespace Editor::DebugServer::Editing::Components::Transform
{
    const Adapter& GetAdapter();
    void MarkHierarchyDirty(Pine::Entity* entity);
}

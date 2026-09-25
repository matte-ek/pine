#pragma once

#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Entity/Entity.hpp"

// The properties panel's component copy and paste.
//
// A copy is a snapshot of the component's SaveData() taken at the moment of copying, so later
// edits to the source, or deleting it, do not change what gets pasted. Like SaveData() itself it
// leaves out the component's active flag, which pasting therefore never changes.
namespace Editor::Clipboard::Component
{
    void Copy(Pine::Component* component);

    bool HasData();
    Pine::ComponentType GetType();

    bool CanPasteValues(const Pine::Component* component);
    bool CanPasteAsNew();

    // Both record an undo step.
    void PasteValues(Pine::Component* component);
    Pine::Component* PasteAsNew(Pine::Entity* entity);
}

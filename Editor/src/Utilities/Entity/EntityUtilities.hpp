#pragma once

#include <vector>

#include "Pine/World/Entity/Entity.hpp"

namespace Editor::Utilities::Entity
{
    // Deletes the entity and its children the way every editor path should: it first drops them from
    // the selection, cancels an entity-list drag holding one of them, and clears any rendering
    // context whose scene camera belongs to one of them, since all three hold raw pointers.
    void DeleteHierarchy(Pine::Entity* root);

    // The entities that have no ancestor in the same list, in their original order. Anything that
    // works on whole hierarchies uses this so a selected child is not handled twice, once on its
    // own and once inside its selected parent.
    std::vector<Pine::Entity*> GetTopmost(const std::vector<Pine::Entity*>& entities);
}

#include "EntityUtilities.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

#include "Gui/Panels/EntityList/EntityListPanel.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    bool HasAncestorIn(const Pine::Entity* entity, const std::vector<Pine::Entity*>& entities)
    {
        for (auto ancestor = entity->GetParent(); ancestor != nullptr; ancestor = ancestor->GetParent())
        {
            if (std::find(entities.begin(), entities.end(), ancestor) != entities.end())
            {
                return true;
            }
        }

        return false;
    }

    void ClearDeletedCamera(Pine::RenderingContext* context, const std::unordered_set<Pine::Entity*>& deleted)
    {
        if (context == nullptr || context->SceneCamera == nullptr)
        {
            return;
        }

        if (deleted.count(context->SceneCamera->GetParent()) != 0)
        {
            context->SceneCamera = nullptr;
        }
    }
}

void Editor::Utilities::Entity::DeleteHierarchy(Pine::Entity* root)
{
    std::vector<Pine::Entity*> hierarchy{ root };

    for (std::size_t index = 0; index < hierarchy.size(); index++)
    {
        const auto& children = hierarchy[index]->GetChildren();

        hierarchy.insert(hierarchy.end(), children.begin(), children.end());
    }

    for (const auto entity : hierarchy)
    {
        Panels::EntityList::CancelEntityDrag(entity);

        if (Selection::IsSelected(entity))
        {
            // AddEntity toggles an existing selection off without disturbing the rest.
            Selection::AddEntity(entity);
        }
    }

    const std::unordered_set<Pine::Entity*> deleted(hierarchy.begin(), hierarchy.end());

    for (const auto context : Pine::RenderManager::GetRenderingContexts())
    {
        ClearDeletedCamera(context, deleted);
    }

    ClearDeletedCamera(Pine::RenderManager::GetDefaultRenderingContext(), deleted);

    if (!Pine::Entities::Delete(root))
    {
        throw std::runtime_error("Could not delete entity hierarchy.");
    }
}

std::vector<Pine::Entity*> Editor::Utilities::Entity::GetTopmost(const std::vector<Pine::Entity*>& entities)
{
    std::vector<Pine::Entity*> topmost;

    for (const auto entity : entities)
    {
        if (!HasAncestorIn(entity, entities))
        {
            topmost.push_back(entity);
        }
    }

    return topmost;
}

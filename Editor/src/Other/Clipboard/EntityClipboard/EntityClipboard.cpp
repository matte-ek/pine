#include "EntityClipboard.hpp"

#include <memory>

#include "Other/Actions/Actions.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Utilities/Entity/EntityUtilities.hpp"

namespace
{
    struct CopiedEntity
    {
        // The entity and its children as Entity::SaveData() writes them, which is also what a
        // Blueprint asset stores.
        Pine::ByteSpan Data;

        bool HasParent = false;
        Pine::UId ParentId;
    };

    std::vector<CopiedEntity> m_CopiedEntities;

    std::vector<CopiedEntity> CopyEntities(const std::vector<Pine::Entity*>& entities)
    {
        std::vector<CopiedEntity> copiedEntities;

        for (const auto entity : Editor::Utilities::Entity::GetTopmost(entities))
        {
            CopiedEntity copiedEntity;

            copiedEntity.Data = entity->SaveData();

            if (const auto parent = entity->GetParent())
            {
                copiedEntity.HasParent = true;
                copiedEntity.ParentId = parent->GetId();
            }

            copiedEntities.push_back(std::move(copiedEntity));
        }

        return copiedEntities;
    }

    std::vector<Pine::Entity*> SpawnEntities(const std::vector<CopiedEntity>& copiedEntities)
    {
        std::vector<Pine::Entity*> spawnedEntities;

        for (const auto& copiedEntity : copiedEntities)
        {
            Pine::Blueprint blueprint;

            blueprint.FromByteSpan(copiedEntity.Data);

            const auto spawnedEntity = blueprint.Spawn();

            blueprint.Dispose();

            if (copiedEntity.HasParent)
            {
                if (const auto parent = Pine::Entities::Find(copiedEntity.ParentId))
                {
                    parent->AddChild(spawnedEntity);
                }
            }

            spawnedEntities.push_back(spawnedEntity);
        }

        if (!spawnedEntities.empty())
        {
            Editor::Actions::RegisterCommand(std::make_unique<Editor::Actions::CreateDeleteEntityCommand>(
                spawnedEntities, Editor::Actions::CommandType::Create));
        }

        return spawnedEntities;
    }
}

void Editor::Clipboard::Entity::Copy(const std::vector<Pine::Entity*>& entities)
{
    m_CopiedEntities = CopyEntities(entities);
}

bool Editor::Clipboard::Entity::HasData()
{
    return !m_CopiedEntities.empty();
}

std::vector<Pine::Entity*> Editor::Clipboard::Entity::Paste()
{
    return SpawnEntities(m_CopiedEntities);
}

std::vector<Pine::Entity*> Editor::Clipboard::Entity::Duplicate(const std::vector<Pine::Entity*>& entities)
{
    return SpawnEntities(CopyEntities(entities));
}

#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace Pine
{

    class Blueprint : public IAsset
    {
    private:
        // Stored entity which is the entity the blueprint is describing. This does not
        // point to any existing entity in the world.
        Pine::Entity* m_Entity = nullptr;

        // Copies entity and component data from the source to the destination.
        // If createInstance is set, it will create the source entities components in the world.
        void CopyEntity(Pine::Entity* dst, const Pine::Entity* src, bool createInstance) const;
    public:
        explicit Blueprint();



        void Dispose() override;
    };

}
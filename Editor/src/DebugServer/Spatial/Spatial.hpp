#pragma once

#include <limits>

#include "../DebugServer.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Pine
{
    class Entity;
}

namespace Editor::DebugServer::Spatial
{
    struct Bounds
    {
        glm::dvec3 Min = glm::dvec3(std::numeric_limits<double>::max());
        glm::dvec3 Max = glm::dvec3(std::numeric_limits<double>::lowest());
        bool Empty = true;

        void Include(const glm::dvec3& point);
    };

    // Shared with camera framing. Measures only this entity's ModelRenderer geometry.
    bool AddModelBounds(Pine::Entity* entity, Bounds& bounds, bool includeInactive = true);
    void AddTerrainBounds(Pine::Entity* entity, Bounds& bounds, bool includeInactive = true);
    nlohmann::json ReadWorldTransform(const Pine::Entity* entity);
    nlohmann::json ReadLocalTransform(const Pine::Entity* entity);
    Response Query(const Request& request);
}

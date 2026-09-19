#include <PxRigidActor.h>
#include <PxScene.h>
#include "Interfaces.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"
#include "Pine/World/Entity/Entity.hpp"

#include <vector>

namespace
{
    // One hit as it crosses to C#, which reassembles it into Pine.Physics.Data.RayCastHit. That
    // type carries a managed Entity reference the engine has no way to write, so the entity's
    // handle travels instead and the managed side resolves it.
    struct RayCastHit
    {
        std::uint64_t EntityHandle;
        Pine::Vector3f Position;
        Pine::Vector3f Normal;
    };

    // The hits of the most recent query. C# cannot allocate its array before it knows how many
    // there are, and the query must not run once per hit, so RayCastQuery runs it and leaves the
    // results here for RayCastGetHit to read out.
    std::vector<RayCastHit> m_RayCastHits;

    int RayCastQuery(Pine::Vector3f origin, Pine::Vector3f direction, float maxDistance, int layerMask)
    {
        m_RayCastHits.clear();

        physx::PxRaycastBuffer result;

        Pine::Physics3D::GetScene()->raycast(
            physx::PxVec3(origin.x, origin.y, origin.z),
            physx::PxVec3(direction.x, direction.y, direction.z),
            maxDistance,
            result,
            physx::PxHitFlag::eDEFAULT);

        for (physx::PxU32 i = 0; i < result.nbTouches; i++)
        {
            const auto touch = result.getTouch(i);
            const auto entity = static_cast<Pine::Entity*>(touch.actor->userData);

            m_RayCastHits.push_back({
                entity->GetScriptHandle()->Id,
                {touch.position.x, touch.position.y, touch.position.z},
                {touch.normal.x, touch.normal.y, touch.normal.z}
            });
        }

        return static_cast<int>(m_RayCastHits.size());
    }

    void RayCastGetHit(const int index, RayCastHit* hit)
    {
        if (index < 0 || index >= static_cast<int>(m_RayCastHits.size()))
        {
            *hit = {};

            return;
        }

        *hit = m_RayCastHits[index];
    }
}

void Pine::Script::Interfaces::Physics::Setup()
{
    Bindings::Register("Pine.Physics.Physics3D::RayCastQuery", RayCastQuery);
    Bindings::Register("Pine.Physics.Physics3D::RayCastGetHit", RayCastGetHit);
}

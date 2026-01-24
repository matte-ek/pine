#include <PxRigidActor.h>
#include <PxScene.h>
#include <mono/metadata/appdomain.h>
#include "Interfaces.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    struct RayCastHit
    {
        MonoObject* Entity;
        Pine::Vector3f Position;
        Pine::Vector3f Normal;
    };

    MonoArray* PhysicsRayCast(Pine::Vector3f origin, Pine::Vector3f direction, float maxDistance, int layerMask)
    {
        physx::PxRaycastBuffer result;

        Pine::Physics3D::GetScene()->raycast(
            physx::PxVec3(origin.x, origin.y, origin.z),
            physx::PxVec3(direction.x, direction.y, direction.z),
            maxDistance,
            result,
            physx::PxHitFlag::eDEFAULT);

        auto arr = mono_array_new(mono_domain_get(), Pine::Script::ObjectFactory::GetRayCastHitClass(), result.nbTouches);

        for (int i = 0; i < result.nbTouches; i++)
        {
            auto entity = static_cast<Pine::Entity*>(result.getTouch(i).actor->userData);
            auto hit = result.getTouch(i);

            RayCastHit hitObj;

            hitObj.Entity = mono_gchandle_get_target(entity->GetScriptHandle()->Handle);
            hitObj.Position = {hit.position.x, hit.position.y, hit.position.z};
            hitObj.Normal = {hit.normal.x, hit.normal.y, hit.normal.z};

            auto obj = mono_value_box(Pine::Script::Runtime::GetDomain(), Pine::Script::ObjectFactory::GetRayCastHitClass(), &hitObj);

            mono_array_setref(arr, i, obj);
        }

        return arr;
    }
}

void Pine::Script::Interfaces::Physics::Setup()
{
    mono_add_internal_call("Pine.Physics.Physics3D::RayCast", reinterpret_cast<void*>(PhysicsRayCast));
}
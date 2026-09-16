#include "RigidBody.hpp"

#include <algorithm>

#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"

Pine::RigidBody::RigidBody()
        : Component(ComponentType::RigidBody)
{
}

physx::PxRigidDynamic * Pine::RigidBody::GetRigidBody() const
{
    if (m_Actor == nullptr || m_ActorIsStatic)
    {
        return nullptr;
    }

    return static_cast<physx::PxRigidDynamic*>(m_Actor);
}

void Pine::RigidBody::ApplyForce(const Vector3f& force, const physx::PxForceMode::Enum mode) const
{
    auto* dynamicActor = GetRigidBody();

    if (dynamicActor == nullptr)
    {
        return;
    }

    dynamicActor->addForce(physx::PxVec3(force.x, force.y, force.z), mode);
}

void Pine::RigidBody::DestroyActor()
{
    if (m_Actor == nullptr)
    {
        return;
    }

    Physics3D::GetScene()->removeActor(*m_Actor);

    m_Actor->release();
    m_Actor = nullptr;
}

void Pine::RigidBody::UpdateColliders()
{
    const auto collider = m_Parent->GetComponent<Collider>();

    if (!collider)
    {
        // If our collider got removed, we'll also have to remove our rigid body as well.
        DestroyActor();

        return;
    }

    m_EngineCollider = collider;
}

void Pine::RigidBody::UpdateBody()
{
    UpdateColliders();

    if (m_EngineCollider == nullptr)
    {
        return;
    }

    // The entity's Static flag means "this transform will not change at runtime", so it forces a
    // static actor regardless of the configured body type. Note that it must not skip actor
    // creation - an entity missing from the physics scene entirely is something nothing else can
    // collide against, and it fails silently.
    const bool isStatic = m_RigidBodyType == RigidBodyType::Static || m_Parent->GetStatic();

    const auto transform = GetParent()->GetTransform();
    const auto position = transform->GetPosition() + m_EngineCollider->GetPosition();
    const auto rotation = transform->GetRotation();

    m_RigidBodyTransform.p.x = position.x;
    m_RigidBodyTransform.p.y = position.y;
    m_RigidBodyTransform.p.z = position.z;

    m_RigidBodyTransform.q.x = rotation.x;
    m_RigidBodyTransform.q.y = rotation.y;
    m_RigidBodyTransform.q.z = rotation.z;
    m_RigidBodyTransform.q.w = rotation.w;

    // Static and dynamic bodies are different PhysX actor classes, so a type change mid-run has
    // to rebuild the actor rather than reconfigure it.
    if (m_Actor != nullptr && m_ActorIsStatic != isStatic)
    {
        DestroyActor();
    }

    if (m_Actor == nullptr)
    {
        CreateActor(isStatic);
    }

    if (m_Actor == nullptr || m_ActorIsStatic)
    {
        // A static actor is placed once, at creation, and never moved again.
        return;
    }

    auto* dynamicActor = static_cast<physx::PxRigidDynamic*>(m_Actor);

    if (m_RigidBodyType == RigidBodyType::Kinematic)
    {
        dynamicActor->setKinematicTarget(m_RigidBodyTransform);
    }
    else
    {
        // scary.
        dynamicActor->setGlobalPose(m_RigidBodyTransform);
    }
}

void Pine::RigidBody::CreateActor(const bool isStatic)
{
    auto* physics = Physics3D::GetPhysics();

    if (isStatic)
    {
        m_Actor = physics->createRigidStatic(m_RigidBodyTransform);
    }
    else
    {
        auto* dynamicActor = physics->createRigidDynamic(m_RigidBodyTransform);

        dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, m_RigidBodyType == RigidBodyType::Kinematic);
        dynamicActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !m_GravityEnabled);

        // Rotation lock
        dynamicActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, m_RotationLock[0]);
        dynamicActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, m_RotationLock[1]);
        dynamicActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, m_RotationLock[2]);

        // Position lock
        dynamicActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X, m_PositionLock[0]);
        dynamicActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y, m_PositionLock[1]);
        dynamicActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z, m_PositionLock[2]);

        if (m_MaxAngularVelocity != 0.f)
        {
            dynamicActor->setMaxAngularVelocity(m_MaxAngularVelocity);
        }

        if (m_MaxLinearVelocity != 0.f)
        {
            dynamicActor->setMaxLinearVelocity(m_MaxLinearVelocity);
        }

        m_Actor = dynamicActor;
    }

    m_ActorIsStatic = isStatic;

    const auto shape = m_EngineCollider->CreateCollisionShape();

    if (shape == nullptr)
    {
        PError(fmt::format("RigidBody::CreateActor(): No collision shape available for entity '{}'.", m_Parent->GetName()));

        m_Actor->release();
        m_Actor = nullptr;

        return;
    }

    m_Actor->attachShape(*shape);
    m_Actor->userData = m_Parent;

    shape->release();

    if (!isStatic)
    {
        // Derive the inertia tensor from the shape, so rotation responds to the body's mass and
        // size instead of keeping PhysX's default (1,1,1). Both need the shape, hence the placement
        // after attachShape(). A trigger shape takes no part in simulation, so it has to be opted
        // in explicitly - otherwise PhysX finds no shape to compute from.
        const auto mass = std::max(m_Mass, MinimumMass);

        physx::PxRigidBodyExt::setMassAndUpdateInertia(*static_cast<physx::PxRigidDynamic*>(m_Actor),
            mass, nullptr, m_EngineCollider->IsTrigger());
    }

    Physics3D::GetScene()->addActor(*m_Actor);
}

void Pine::RigidBody::SetRigidBodyType(const RigidBodyType type)
{
    m_RigidBodyType = type;
}

Pine::RigidBodyType Pine::RigidBody::GetRigidBodyType() const
{
    return m_RigidBodyType;
}

void Pine::RigidBody::SetMass(const float mass)
{
    m_Mass = mass;
}

float Pine::RigidBody::GetMass() const
{
    return m_Mass;
}

void Pine::RigidBody::SetGravityEnabled(const bool value)
{
    m_GravityEnabled = value;
}

bool Pine::RigidBody::GetGravityEnabled() const
{
    return m_GravityEnabled;
}

void Pine::RigidBody::SetMaxLinearVelocity(const float maxLinearVelocity)
{
    m_MaxLinearVelocity = maxLinearVelocity;
}

float Pine::RigidBody::GetMaxLinearVelocity() const
{
    return m_MaxLinearVelocity;
}

void Pine::RigidBody::SetMaxAngularVelocity(const float maxAngularVelocity)
{
    m_MaxAngularVelocity = maxAngularVelocity;
}

float Pine::RigidBody::GetMaxAngularVelocity() const
{
    return m_MaxAngularVelocity;
}

std::array<bool, 3> Pine::RigidBody::GetRotationLock() const
{
    return m_RotationLock;
}

void Pine::RigidBody::SetRotationLock(const std::array<bool, 3> value)
{
    m_RotationLock = value;
}

std::array<bool, 3> Pine::RigidBody::GetPositionLock() const
{
    return m_PositionLock;
}

void Pine::RigidBody::SetPositionLock(const std::array<bool, 3> value)
{
    m_PositionLock = value;
}

bool Pine::RigidBody::IsColliderAttached(const Collider *collider) const
{
    return m_EngineCollider == collider;
}

void Pine::RigidBody::OnPrePhysicsUpdate()
{
    UpdateBody();
}

void Pine::RigidBody::OnPostPhysicsUpdate()
{
    // Only a simulated dynamic actor moves on its own; everything else is driven from the
    // transform, so reading the pose back would just fight whoever wrote it.
    if (m_Actor == nullptr || m_ActorIsStatic)
        return;
    if (m_RigidBodyType != RigidBodyType::Dynamic)
        return;

    const auto transform = GetParent()->GetTransform();
    const auto pose = m_Actor->getGlobalPose();

    transform->SetLocalPosition(Vector3f(pose.p.x, pose.p.y, pose.p.z) - m_EngineCollider->GetPosition());
    transform->SetLocalRotation({pose.q.w, pose.q.x, pose.q.y, pose.q.z});
}

void Pine::RigidBody::OnCopied()
{
    Component::OnCopied();

    // The copy must create and own its own PhysX actor, rather than inherit the source's pointer.
    m_Actor = nullptr;
    m_ActorIsStatic = false;
    m_EngineCollider = nullptr;
}

void Pine::RigidBody::OnDestroyed()
{
    Component::OnDestroyed();

    DestroyActor();
}

void Pine::RigidBody::LoadData(const ByteSpan& span)
{
    RigidBodySerializer serializer;

    serializer.Read(span);

    serializer.Type.Read(m_RigidBodyType);
    serializer.Mass.Read(m_Mass);
    serializer.GravityEnabled.Read(m_GravityEnabled);
    serializer.PositionLock.Read(m_PositionLock);
    serializer.RotationLock.Read(m_RotationLock);
    serializer.MaxAngularVelocity.Read(m_MaxAngularVelocity);
    serializer.MaxLinearVelocity.Read(m_MaxLinearVelocity);
}

Pine::ByteSpan Pine::RigidBody::SaveData()
{
    RigidBodySerializer serializer;

    serializer.Type.Write(m_RigidBodyType);
    serializer.Mass.Write(m_Mass);
    serializer.GravityEnabled.Write(m_GravityEnabled);
    serializer.PositionLock.Write(m_PositionLock);
    serializer.RotationLock.Write(m_RotationLock);
    serializer.MaxAngularVelocity.Write(m_MaxAngularVelocity);
    serializer.MaxLinearVelocity.Write(m_MaxLinearVelocity);

    return serializer.Write();
}

#include "RigidBody.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::RigidBody::RigidBody()
        : IComponent(ComponentType::RigidBody)
{
}

physx::PxRigidDynamic * Pine::RigidBody::GetRigidBody() const
{
    return m_RigidBody;
}

void Pine::RigidBody::UpdateColliders()
{
    const auto collider = m_Parent->GetComponent<Collider>();

    if (!collider)
    {
        // If our collider got removed, we'll also have to remove our rigid body as well.
        if (m_RigidBody != nullptr)
        {
            Physics3D::GetScene()->removeActor(*m_RigidBody);

            m_RigidBody->release();
            m_RigidBody = nullptr;
        }

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

    const auto transform = GetParent()->GetTransform();
    const auto position = transform->GetPosition();
    const auto rotation = transform->GetRotation();

    m_RigidBodyTransform.p.x = position.x;
    m_RigidBodyTransform.p.y = position.y;
    m_RigidBodyTransform.p.z = position.z;

    m_RigidBodyTransform.q.x = rotation.x;
    m_RigidBodyTransform.q.y = rotation.y;
    m_RigidBodyTransform.q.z = rotation.z;
    m_RigidBodyTransform.q.w = rotation.w;

    if (m_RigidBody == nullptr)
    {
        m_RigidBody = Physics3D::GetPhysics()->createRigidDynamic(m_RigidBodyTransform);
        m_RigidBody->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, m_RigidBodyType == RigidBodyType::Kinematic);

        const auto shape = m_EngineCollider->CreateCollisionShape();

        m_RigidBody->attachShape(*shape);

        shape->release();

        Physics3D::GetScene()->addActor(*m_RigidBody);
    }

    m_RigidBody->setMass(m_Mass);

    if (m_RigidBodyType == RigidBodyType::Kinematic)
    {
        m_RigidBody->setKinematicTarget(m_RigidBodyTransform);
    }
    else
    {
        // scary.
        m_RigidBody->setGlobalPose(m_RigidBodyTransform);
    }
}

void Pine::RigidBody::SetRigidBodyType(RigidBodyType type)
{
    m_RigidBodyType = type;
}

Pine::RigidBodyType Pine::RigidBody::GetRigidBodyType() const
{
    return m_RigidBodyType;
}

void Pine::RigidBody::SetMass(float mass)
{
    m_Mass = mass;
}

float Pine::RigidBody::GetMass() const
{
    return m_Mass;
}

void Pine::RigidBody::SetGravityEnabled(bool value)
{
    m_GravityEnabled = value;
}

bool Pine::RigidBody::GetGravityEnabled() const
{
    return m_GravityEnabled;
}

bool Pine::RigidBody::IsColliderAttached(const Collider *collider) const
{
    return m_EngineCollider == collider;
}

void Pine::RigidBody::OnPrePhysicsUpdate()
{
    if (m_Parent->GetStatic())
    {
        return;
    }

    UpdateBody();
}

void Pine::RigidBody::OnPostPhysicsUpdate()
{
    if (m_Parent->GetStatic())
        return;
    if (m_RigidBody == nullptr)
        return;

    const auto transform = GetParent()->GetTransform();
    const auto position = m_RigidBody->getGlobalPose().p;
    const auto rotation = m_RigidBody->getGlobalPose().q;

    transform->LocalPosition = {position.x, position.y, position.z};
    transform->LocalRotation = {rotation.w, rotation.x, rotation.y, rotation.z};
}

void Pine::RigidBody::OnCopied()
{
    IComponent::OnCopied();
}

void Pine::RigidBody::OnDestroyed()
{
    IComponent::OnDestroyed();

    if (m_RigidBody)
    {
        Physics3D::GetScene()->removeActor(*m_RigidBody);

        m_RigidBody->release();
        m_RigidBody = nullptr;
    }
}

void Pine::RigidBody::LoadData(const nlohmann::json &j)
{
    Serialization::LoadValue(j, "mass", m_Mass);
    Serialization::LoadValue(j, "grav", m_GravityEnabled);
    Serialization::LoadValue(j, "rtype", m_RigidBodyType);
}

void Pine::RigidBody::SaveData(nlohmann::json &j)
{
    j["mass"] = m_Mass;
    j["grav"] = m_GravityEnabled;
    j["rtype"] = m_RigidBodyType;
}
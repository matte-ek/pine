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

void Pine::RigidBody::UpdateColliders()
{
    const auto collider = m_Parent->GetComponent<Collider>();

    if (!collider)
    {
        /*
        if (m_RigidBody)
        {
            //Physics3D::GetScene()->removeActor(*m_RigidBody);

            m_RigidBody->release();
            m_RigidBody = nullptr;
        }
        */

        return;
    }

    m_EngineCollider = collider;
}

void Pine::RigidBody::UpdateRigidBodyProperties()
{
    const auto transform = GetParent()->GetTransform();

    UpdateColliders();
}

/*
physx::PxRigidDynamic *Pine::RigidBody::GetRigidBody() const
{
    return m_RigidBody;
}
*/

void Pine::RigidBody::SetRigidBodyType(Pine::RigidBodyType type)
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

void Pine::RigidBody::DetachCollider()
{
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

    UpdateRigidBodyProperties();
}

void Pine::RigidBody::OnPostPhysicsUpdate()
{
    if (m_Parent->GetStatic())
        return;
}

void Pine::RigidBody::OnCopied()
{
    IComponent::OnCopied();
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
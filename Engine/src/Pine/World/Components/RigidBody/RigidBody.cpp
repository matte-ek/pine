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
    const auto collider = m_Parent->GetComponent<Pine::Collider>();

    if (!collider)
    {
        if (m_Collider)
            m_RigidBody->removeCollider(m_Collider);

        m_EngineCollider = nullptr;
        m_Collider = nullptr;

        return;
    }

    collider->OnPrePhysicsUpdate();

    m_EngineCollider = collider;

    if (collider->PollShapeUpdated())
    {
        if (m_Collider)
            m_RigidBody->removeCollider(m_Collider);

        m_Collider = nullptr;
    }

    if (!m_Collider)
        m_Collider = m_RigidBody->addCollider(collider->GetCollisionShape(), m_ColliderTransform);

    if (m_Collider)
    {
        const auto transform = m_Parent->GetTransform();

        reactphysics3d::Transform tr;

        tr.identity();

        const auto rot = transform->GetRotation();

        tr.setPosition(reactphysics3d::Vector3(collider->GetPosition().x, collider->GetPosition().y, collider->GetPosition().z));
        tr.setOrientation(reactphysics3d::Quaternion(rot.x, rot.y, rot.z, rot.w));

        m_Collider->setLocalToBodyTransform(tr);
        m_Collider->getMaterial().setBounciness(0);
    }
}

void Pine::RigidBody::UpdateRigidBodyProperties()
{
    const auto transform = GetParent()->GetTransform();

    reactphysics3d::Transform tr;

    const auto position = transform->GetPosition();
    const auto rotation = transform->GetRotation();

    tr.setPosition(reactphysics3d::Vector3(position.x, position.y, position.z));
    tr.setOrientation(reactphysics3d::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w));

    m_RigidBody->setMass(m_Mass);
    m_RigidBody->enableGravity(m_GravityEnabled);
    m_RigidBody->setTransform(tr);

    m_RigidBody->setAngularLockAxisFactor(reactphysics3d::Vector3(m_RotationLock[0] ? 0.f : 1.f, m_RotationLock[1] ? 0.f : 1.f, m_RotationLock[2] ? 0.f : 1.f));

    m_RigidBody->setLinearDamping(.2f);

    switch (m_RigidBodyType)
    {
        case RigidBodyType::Static:
            m_RigidBody->setType(reactphysics3d::BodyType::STATIC);
            break;
        case RigidBodyType::Kinematic:
            m_RigidBody->setType(reactphysics3d::BodyType::KINEMATIC);
            break;
        case RigidBodyType::Dynamic:
            m_RigidBody->setType(reactphysics3d::BodyType::DYNAMIC);
            break;
    }

    UpdateColliders();
}

reactphysics3d::RigidBody *Pine::RigidBody::GetRigidBody() const
{
    return m_RigidBody;
}

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

void Pine::RigidBody::SetRotationLock(std::array<bool, 3> rot)
{
    m_RotationLock = rot;
}

const std::array<bool, 3> &Pine::RigidBody::GetRotationLock() const
{
    return m_RotationLock;
}

void Pine::RigidBody::DetachCollider()
{
    if (m_Collider && m_RigidBody)
        m_RigidBody->removeCollider(m_Collider);

    m_EngineCollider = nullptr;
    m_Collider = nullptr;
}

bool Pine::RigidBody::IsColliderAttached(Pine::Collider *collider) const
{
    return m_EngineCollider == collider;
}

void Pine::RigidBody::OnPrePhysicsUpdate()
{
    if (!m_RigidBody)
    {
        m_RigidBody = Physics3D::GetWorld()->createRigidBody(m_RigidBodyTransform);
        m_RigidBody->setType(reactphysics3d::BodyType::DYNAMIC);

        UpdateRigidBodyProperties();
    }

    if (m_Parent->GetStatic())
        return;

    UpdateRigidBodyProperties();
}

void Pine::RigidBody::OnPostPhysicsUpdate()
{
    if (m_Parent->GetStatic())
        return;
    if (!m_RigidBody)
        return;

    const auto transform = GetParent()->GetTransform();
    const auto &physTransform = m_RigidBody->getTransform();

    transform->LocalRotation = glm::quat(physTransform.getOrientation().w, physTransform.getOrientation().x, physTransform.getOrientation().y, physTransform.getOrientation().z);
    transform->LocalPosition = glm::vec3(physTransform.getPosition().x, physTransform.getPosition().y, physTransform.getPosition().z);
}

void Pine::RigidBody::OnCopied()
{
    m_RigidBody = nullptr;
}

void Pine::RigidBody::LoadData(const nlohmann::json &j)
{
    Serialization::LoadValue(j, "mass", m_Mass);
    Serialization::LoadValue(j, "grav", m_GravityEnabled);
    Serialization::LoadValue(j, "rtype", m_RigidBodyType);

    Serialization::LoadValue(j, "lrx", m_RotationLock[0]);
    Serialization::LoadValue(j, "lry", m_RotationLock[1]);
    Serialization::LoadValue(j, "lrz", m_RotationLock[2]);
}

void Pine::RigidBody::SaveData(nlohmann::json &j)
{
    j["mass"] = m_Mass;
    j["grav"] = m_GravityEnabled;
    j["rtype"] = m_RigidBodyType;

    j["lrx"] = m_RotationLock[0];
    j["lry"] = m_RotationLock[1];
    j["lrz"] = m_RotationLock[2];
}
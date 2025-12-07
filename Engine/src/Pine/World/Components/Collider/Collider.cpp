#include "Collider.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::Collider::Collider::Collider()
        : IComponent(ComponentType::Collider)
{
}

void Pine::Collider::UpdateBody()
{
    const bool hasRigidBody = m_Parent->GetComponent<RigidBody>() != nullptr;

    if (hasRigidBody)
    {
        if (m_CollisionRigidBody)
        {
            Physics3D::GetScene()->removeActor(*m_CollisionRigidBody);

            m_CollisionRigidBody->release();
            m_CollisionRigidBody = nullptr;
        }

        return;
    }

    if (!m_CollisionRigidBody)
    {
        const auto transform = GetParent()->GetTransform();
        const auto position = transform->GetPosition();
        const auto rotation = transform->GetRotation();

        m_Transform.p.x = position.x;
        m_Transform.p.y = position.y;
        m_Transform.p.z = position.z;

        m_Transform.q.x = rotation.x;
        m_Transform.q.y = rotation.y;
        m_Transform.q.z = rotation.z;
        m_Transform.q.w = rotation.w;

        m_CollisionRigidBody = Physics3D::GetPhysics()->createRigidStatic(m_Transform);

        const auto collisionShape = CreateCollisionShape();
        if (!collisionShape)
        {
            Log::Error("Collider::UpdateBody(): Failed to create collision body, no shape available.");
            return;
        }

        m_CollisionRigidBody->attachShape(*collisionShape);

        collisionShape->release();

        Physics3D::GetScene()->addActor(*m_CollisionRigidBody);
    }
}

void Pine::Collider::SetColliderType(const ColliderType type)
{
    m_ColliderType = type;

    if (m_Standalone || !m_CollisionRigidBody)
    {
        return;
    }

    Physics3D::GetScene()->removeActor(*m_CollisionRigidBody);

    m_CollisionRigidBody->release();
    m_CollisionRigidBody = nullptr;
}

Pine::ColliderType Pine::Collider::GetColliderType() const
{
    return m_ColliderType;
}

void Pine::Collider::SetPosition(Vector3f position)
{
    m_Position = position;
}

const Pine::Vector3f &Pine::Collider::GetPosition() const
{
    return m_Position;
}

void Pine::Collider::SetSize(Vector3f size)
{
    m_Size = size;
}

const Pine::Vector3f &Pine::Collider::GetSize() const
{
    return m_Size;
}

void Pine::Collider::SetRadius(float radius)
{
    m_Size.x = radius;
}

float Pine::Collider::GetRadius() const
{
    return m_Size.x;
}

void Pine::Collider::SetHeight(float height)
{
    m_Size.y = height;
}

float Pine::Collider::GetHeight() const
{
    return m_Size.y;
}

void Pine::Collider::Reset()
{
    if (!m_CollisionRigidBody)
    {
        return;
    }

    Physics3D::GetScene()->removeActor(*m_CollisionRigidBody);

    m_CollisionRigidBody->release();
    m_CollisionRigidBody = nullptr;
}

physx::PxShape * Pine::Collider::CreateCollisionShape() const
{
    auto size = m_Size * GetParent()->GetTransform()->GetScale();

    switch (m_ColliderType)
    {
    case ColliderType::Box:
        return Physics3D::GetPhysics()->createShape(physx::PxBoxGeometry(size.x, size.y, size.z), *Physics3D::GetDefaultMaterial());
    case ColliderType::Sphere:
        return Physics3D::GetPhysics()->createShape(physx::PxSphereGeometry(size.x), *Physics3D::GetDefaultMaterial());
    case ColliderType::Capsule:
        return Physics3D::GetPhysics()->createShape(physx::PxCapsuleGeometry(size.x, size.y), *Physics3D::GetDefaultMaterial());
    default:
        break;
    }

    return nullptr;
}

void Pine::Collider::OnPrePhysicsUpdate()
{
    if (m_Standalone) return;

    UpdateBody();
}

void Pine::Collider::OnDestroyed()
{
    IComponent::OnDestroyed();

    if (m_CollisionRigidBody)
    {
        Physics3D::GetScene()->removeActor(*m_CollisionRigidBody);

        m_CollisionRigidBody->release();
        m_CollisionRigidBody = nullptr;
    }
}

void Pine::Collider::OnCopied()
{
    IComponent::OnCopied();

    m_CollisionRigidBody = nullptr;
}

void Pine::Collider::LoadData(const nlohmann::json &j)
{
    Serialization::LoadVector3(j, "pos", m_Position);
    Serialization::LoadVector3(j, "size", m_Size);
    Serialization::LoadValue(j, "ctype", m_ColliderType);
}

void Pine::Collider::SaveData(nlohmann::json &j)
{
    j["pos"] = Serialization::StoreVector3(m_Position);
    j["size"] = Serialization::StoreVector3(m_Size);
    j["ctype"] = m_ColliderType;
}
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
        const auto position = transform->GetPosition() + m_Position;
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
        m_CollisionRigidBody->userData = m_Parent;

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

void Pine::Collider::SetLayer(std::uint32_t layer)
{
    m_Layer = layer;
}

std::uint32_t Pine::Collider::GetLayer() const
{
    return m_Layer;
}

void Pine::Collider::SetLayerMask(std::uint32_t includeLayers)
{
    m_LayerMask = includeLayers;
}

std::uint32_t Pine::Collider::GetLayerMask() const
{
    return m_LayerMask;
}

void Pine::Collider::SetIsTrigger(bool isTrigger)
{
    m_IsTrigger = isTrigger;
}

bool Pine::Collider::IsTrigger() const
{
    return m_IsTrigger;
}

void Pine::Collider::SetTriggerMask(std::uint32_t mask)
{
    m_TriggerMask = mask;
}

std::uint32_t Pine::Collider::GetTriggerMask() const
{
    return m_TriggerMask;
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
    physx::PxShape* shape = nullptr;

    switch (m_ColliderType)
    {
    case ColliderType::Box:
        shape = Physics3D::GetPhysics()->createShape(physx::PxBoxGeometry(size.x, size.y, size.z), *Physics3D::GetDefaultMaterial());
        break;
    case ColliderType::Sphere:
        shape = Physics3D::GetPhysics()->createShape(physx::PxSphereGeometry(size.x), *Physics3D::GetDefaultMaterial());
        break;
    case ColliderType::Capsule:
        shape = Physics3D::GetPhysics()->createShape(physx::PxCapsuleGeometry(size.y, size.x), *Physics3D::GetDefaultMaterial());
    default:
        break;
    }

    if (shape)
    {
        shape->setSimulationFilterData(GetFilterData());
        shape->setQueryFilterData(GetFilterData());

        shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_IsTrigger);
        shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_IsTrigger);
    }

    return shape;
}

physx::PxFilterData Pine::Collider::GetFilterData() const
{
    physx::PxFilterData ret;

    ret.word0 = m_Layer;
    ret.word1 = m_LayerMask;
    ret.word2 = m_IsTrigger ? m_TriggerMask : 0;
    ret.word3 = 0;

    return ret;
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
    Serialization::LoadValue(j, "lay", m_Layer);
    Serialization::LoadValue(j, "lmask", m_LayerMask);
    Serialization::LoadValue(j, "trig", m_IsTrigger);
    Serialization::LoadValue(j, "trigm", m_TriggerMask);
}

void Pine::Collider::SaveData(nlohmann::json &j)
{
    j["pos"] = Serialization::StoreVector3(m_Position);
    j["size"] = Serialization::StoreVector3(m_Size);
    j["ctype"] = m_ColliderType;
    j["lay"] = m_Layer;
    j["lmask"] = m_LayerMask;
    j["trig"] = m_IsTrigger;
    j["trigm"] = m_TriggerMask;
}
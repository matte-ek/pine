#include "Collider.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Physics/Physics3D/TerrainCollision/TerrainCollision.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"

Pine::Collider::Collider::Collider()
        : Component(ComponentType::Collider)
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

        // A height field has one surface per column by construction, so a rotated terrain is not
        // something either the asset or the renderer can represent - Rendering::TerrainRenderer
        // places its chunks by position alone. Rotating the collision would make the ground you
        // walk on disagree with the ground you see, which is the one thing terrain collision has to
        // get right, so this ignores the rotation for exactly the same reason.
        const auto rotation = m_ColliderType == ColliderType::HeightField
            ? Quaternion(1.f, 0.f, 0.f, 0.f)
            : transform->GetRotation();

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
            // Reported once rather than on every physics update. A height field collider whose
            // terrain has not been assigned yet is a normal editing state, and it is retried every
            // update on purpose - so that the collider starts working the moment a terrain appears
            // - but saying so 120 times a second would bury everything else in the log.
            if (!m_ReportedMissingShape)
            {
                PError("Collider::UpdateBody(): Failed to create collision body, no shape available.");

                m_ReportedMissingShape = true;
            }

            m_CollisionRigidBody->release();
            m_CollisionRigidBody = nullptr;
            return;
        }

        m_ReportedMissingShape = false;

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

void Pine::Collider::SetPosition(const Vector3f position)
{
    m_Position = position;
}

const Pine::Vector3f &Pine::Collider::GetPosition() const
{
    return m_Position;
}

void Pine::Collider::SetSize(const Vector3f size)
{
    m_Size = size;
}

const Pine::Vector3f &Pine::Collider::GetSize() const
{
    return m_Size;
}

void Pine::Collider::SetRadius(const float radius)
{
    m_Size.x = radius;
}

float Pine::Collider::GetRadius() const
{
    return m_Size.x;
}

void Pine::Collider::SetHeight(const float height)
{
    m_Size.y = height;
}

float Pine::Collider::GetHeight() const
{
    return m_Size.y;
}

void Pine::Collider::SetLayer(const std::uint32_t layer)
{
    m_Layer = layer;
}

std::uint32_t Pine::Collider::GetLayer() const
{
    return m_Layer;
}

void Pine::Collider::SetLayerMask(const std::uint32_t includeLayers)
{
    m_LayerMask = includeLayers;
}

std::uint32_t Pine::Collider::GetLayerMask() const
{
    return m_LayerMask;
}

void Pine::Collider::SetIsTrigger(const bool isTrigger)
{
    m_IsTrigger = isTrigger;
}

bool Pine::Collider::IsTrigger() const
{
    return m_IsTrigger;
}

void Pine::Collider::SetTriggerMask(const std::uint32_t mask)
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

// Geometry from a sibling component rather than from this collider's own fields. That is not a
// terrain special case: ColliderType::ConvexMesh and ConcaveMesh will have to read their mesh off
// the sibling ModelRenderer in exactly this way, which is why Collider.cpp already includes it.
physx::PxShape * Pine::Collider::CreateHeightFieldShape() const
{
    const auto terrainRenderer = m_Parent->GetComponent<TerrainRendererComponent>();

    if (terrainRenderer == nullptr || terrainRenderer->GetTerrain() == nullptr)
    {
        return nullptr;
    }

    return Physics3D::TerrainCollision::CreateShape(*terrainRenderer->GetTerrain(), *Physics3D::GetDefaultMaterial());
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
        shape = Physics3D::GetPhysics()->createShape(physx::PxCapsuleGeometry(size.x, size.y), *Physics3D::GetDefaultMaterial());
        break;
    case ColliderType::HeightField:
        shape = CreateHeightFieldShape();
        break;
    default:
        break;
    }

    if (shape)
    {
        shape->setSimulationFilterData(GetFilterData());
        shape->setQueryFilterData(GetFilterData());

        // PhysX forbids a shape from being both a trigger and a simulation shape.
        shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_IsTrigger);
        shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_IsTrigger);

        if (m_ColliderType == ColliderType::Capsule)
        {
            physx::PxTransform relativePose(physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0, 0, 1)));

            shape->setLocalPose(relativePose);
        }
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
    Component::OnDestroyed();

    if (m_CollisionRigidBody)
    {
        Physics3D::GetScene()->removeActor(*m_CollisionRigidBody);

        m_CollisionRigidBody->release();
        m_CollisionRigidBody = nullptr;
    }
}

void Pine::Collider::OnCopied()
{
    Component::OnCopied();

    m_CollisionRigidBody = nullptr;
}

void Pine::Collider::LoadData(const ByteSpan& span)
{
    ColliderSerializer serializer;

    serializer.Read(span);

    serializer.Type.Read(m_ColliderType);
    serializer.Position.Read(m_Position);
    serializer.Size.Read(m_Size);
    serializer.Layer.Read(m_Layer);
    serializer.LayerMask.Read(m_LayerMask);
    serializer.IsTrigger.Read(m_IsTrigger);
    serializer.TriggerMask.Read(m_TriggerMask);
}

Pine::ByteSpan Pine::Collider::SaveData()
{
    ColliderSerializer serializer;

    serializer.Type.Write(m_ColliderType);
    serializer.Position.Write(m_Position);
    serializer.Size.Write(m_Size);
    serializer.Layer.Write(m_Layer);
    serializer.LayerMask.Write(m_LayerMask);
    serializer.IsTrigger.Write(m_IsTrigger);
    serializer.TriggerMask.Write(m_TriggerMask);

    return serializer.Write();
}

#include "Transform.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;

namespace
{
}

const Transform* Transform::GetParentTransform() const
{
    // m_Parent is the entity this transform belongs to, so the parent transform is that entity's
    // parent's.
    if (m_Parent == nullptr || m_Parent->GetParent() == nullptr)
    {
        return nullptr;
    }

    return m_Parent->GetParent()->GetTransform();
}

void Transform::UpdateWorldTransform() const
{
    if (!m_IsWorldStale)
    {
        return;
    }

    const auto parent = GetParentTransform();

    if (parent == nullptr)
    {
        m_Position = m_LocalPosition;
        m_Rotation = m_LocalRotation;
        m_Scale = m_LocalScale;
    }
    else
    {
        const auto parentPosition = parent->GetPosition();
        const auto parentRotation = parent->GetRotation();
        const auto parentScale = parent->GetScale();

        m_Position = parentPosition + parentRotation * (parentScale * m_LocalPosition);
        m_Rotation = parentRotation * m_LocalRotation;
        m_Scale = parentScale * m_LocalScale;
    }

    m_TransformationMatrix = Matrix4f(1.f);

    m_TransformationMatrix = translate(m_TransformationMatrix, m_Position);
    m_TransformationMatrix *= toMat4(m_Rotation);
    m_TransformationMatrix = scale(m_TransformationMatrix, m_Scale);

    m_IsWorldStale = false;
}

Transform::Transform() :
    Component(ComponentType::Transform)
{
}

void Transform::SetDirty()
{
    m_IsDirty = true;
    m_IsWorldStale = true;

    if (m_Parent == nullptr)
    {
        return;
    }

    for (const auto child : m_Parent->GetChildren())
    {
        // Entity::LoadData attaches a child before creating its components.
        if (child->GetComponents().empty())
        {
            continue;
        }

        child->GetTransform()->SetDirty();
    }
}

bool Transform::IsDirty() const
{
    return m_IsDirty;
}

void Transform::OnCreated()
{
    Component::OnCreated();

    // Entity::LoadData can attach a new transform to an entity whose children were placed by the
    // transform it replaced.
    SetDirty();
}

void Transform::OnRender(float deltaTime)
{
    m_IsDirty = false;
}

void Transform::LoadData(const ByteSpan& span)
{
    TransformSerializer serializer;

    serializer.Read(span);

    serializer.LocalPosition.Read(m_LocalPosition);
    serializer.LocalRotation.Read(m_LocalRotation);
    serializer.LocalScale.Read(m_LocalScale);

    SetDirty();
}

ByteSpan Transform::SaveData()
{
    TransformSerializer serializer;

    serializer.LocalPosition.Write(m_LocalPosition);
    serializer.LocalRotation.Write(m_LocalRotation);
    serializer.LocalScale.Write(m_LocalScale);

    return serializer.Write();
}

const Vector3f& Transform::GetLocalPosition() const
{
    return m_LocalPosition;
}

void Transform::SetLocalPosition(const Vector3f& position)
{
    m_LocalPosition = position;
    SetDirty();
}

const Quaternion& Transform::GetLocalRotation() const
{
    return m_LocalRotation;
}

void Transform::SetLocalRotation(const Quaternion& rotation)
{
    m_LocalRotation = rotation;
    SetDirty();
}

const Vector3f& Transform::GetLocalScale() const
{
    return m_LocalScale;
}

void Transform::SetLocalScale(const Vector3f& scale)
{
    m_LocalScale = scale;
    SetDirty();
}

Vector3f Transform::GetPosition() const
{
    UpdateWorldTransform();

    return m_Position;
}

void Transform::SetPosition(const Vector3f& position)
{
    const auto parent = GetParentTransform();

    if (parent == nullptr)
    {
        SetLocalPosition(position);
        return;
    }

    // Undoes the parent's translation, rotation and scale, in the reverse of the order
    // UpdateWorldTransform() applies them.
    const auto offset = inverse(parent->GetRotation()) * (position - parent->GetPosition());

    SetLocalPosition(offset / parent->GetScale());
}

Quaternion Transform::GetRotation() const
{
    UpdateWorldTransform();

    return m_Rotation;
}

void Transform::SetRotation(const Quaternion& rotation)
{
    const auto parent = GetParentTransform();

    SetLocalRotation(parent == nullptr ? rotation : inverse(parent->GetRotation()) * rotation);
}

Vector3f Transform::GetScale() const
{
    UpdateWorldTransform();

    return m_Scale;
}

void Transform::SetScale(const Vector3f& scale)
{
    const auto parent = GetParentTransform();

    SetLocalScale(parent == nullptr ? scale : scale / parent->GetScale());
}

// World-space basis vectors, so these use GetRotation() and not m_LocalRotation. For an entity with
// no parent the two are identical, but for a parented one the local rotation ignores everything the
// parent contributes - a camera childed to a player would report the direction it faces *relative to
// the player* rather than the direction it actually looks.
Vector3f Transform::GetForward() const
{
    return GetRotation() * Vector3f(0.f, 0.f, -1.f);
}

Vector3f Transform::GetRight() const
{
    return GetRotation() * Vector3f(1.f, 0.f, 0.f);
}

Vector3f Transform::GetUp() const
{
    return GetRotation() * Vector3f(0.f, 1.f, 0.f);
}

const Matrix4f &Transform::GetTransformationMatrix() const
{
    UpdateWorldTransform();

    return m_TransformationMatrix;
}

Vector3f Transform::GetEulerAngles() const
{
    return degrees(eulerAngles(m_LocalRotation));
}

void Transform::SetEulerAngles(const Vector3f angle)
{
    m_LocalRotation = glm::quat(radians(angle));
    SetDirty();
}

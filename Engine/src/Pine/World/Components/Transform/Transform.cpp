#include "Transform.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;

void Transform::CalculateTransformationMatrix()
{
    m_TransformationMatrix = Matrix4f(1.f);

    m_TransformationMatrix = translate(m_TransformationMatrix, GetPosition());
    m_TransformationMatrix *= toMat4(GetRotation());
    m_TransformationMatrix = scale(m_TransformationMatrix, GetScale());

    m_IsDirty = false;
}

Transform::Transform() :
    IComponent(ComponentType::Transform)
{
}

void Transform::SetDirty()
{
    m_IsDirty = true;
}

bool Transform::IsDirty() const
{
    return m_IsDirty;
}

void Transform::OnRender(float deltaTime)
{
    if (!m_IsDirty)
    {
        return;
    }

    CalculateTransformationMatrix();
}

void Transform::LoadData(const nlohmann::json &j)
{
    Serialization::LoadVector3(j, "pos", m_LocalPosition);
    Serialization::LoadQuaternion(j, "rot", m_LocalRotation);
    Serialization::LoadVector3(j, "scl", m_LocalScale);

    m_IsDirty = true;
}

void Transform::SaveData(nlohmann::json &j)
{
    j["pos"] = Serialization::StoreVector3(m_LocalPosition);
    j["rot"] = Serialization::StoreQuaternion(m_LocalRotation);
    j["scl"] = Serialization::StoreVector3(m_LocalScale);
}

const Vector3f& Transform::GetLocalPosition() const
{
    return m_LocalPosition;
}

void Transform::SetLocalPosition(const Vector3f& position)
{
    m_LocalPosition = position;
    m_IsDirty = true;
}

const Quaternion& Transform::GetLocalRotation() const
{
    return m_LocalRotation;
}

void Transform::SetLocalRotation(const Quaternion& rotation)
{
    m_LocalRotation = rotation;
    m_IsDirty = true;
}

const Vector3f& Transform::GetLocalScale() const
{
    return m_LocalScale;
}

void Transform::SetLocalScale(const Vector3f& scale)
{
    m_LocalScale = scale;
    m_IsDirty = true;
}

Vector3f Transform::GetPosition() const
{
    Vector3f position = m_LocalPosition;

    if (m_Parent->GetParent() != nullptr)
    {
        position += m_Parent->GetParent()->GetTransform()->GetPosition();
    }

    return position;
}

Quaternion Transform::GetRotation() const
{
    Quaternion rotation = m_LocalRotation;

    if (m_Parent->GetParent() != nullptr)
    {
        rotation = m_Parent->GetParent()->GetTransform()->GetRotation();
        rotation *= m_LocalRotation;
    }

    return rotation;
}

Vector3f Transform::GetScale() const
{
    Vector3f scale = m_LocalScale;

    if (m_Parent->GetParent() != nullptr)
    {
        scale *= m_Parent->GetParent()->GetTransform()->GetScale();
    }

    return scale;
}

Vector3f Transform::GetForward() const
{
    return m_LocalRotation * Vector3f(0.f, 0.f, -1.f);
}

Vector3f Transform::GetRight() const
{
    return m_LocalRotation * Vector3f(1.f, 0.f, 0.f);
}

Vector3f Transform::GetUp() const
{
    return m_LocalRotation * Vector3f(0.f, 1.f, 0.f);
}

const Matrix4f &Transform::GetTransformationMatrix() const
{
    return m_TransformationMatrix;
}

Vector3f Transform::GetEulerAngles() const
{
    return degrees(eulerAngles(m_LocalRotation));
}

void Transform::SetEulerAngles(Vector3f angle)
{
    m_LocalRotation = glm::quat(radians(angle));
    m_IsDirty = true;
}

#include "Transform.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;

Transform::Transform() :
        IComponent(ComponentType::Transform)
{
}

void Transform::CalculateTransformationMatrix()
{
    m_TransformationMatrix = Matrix4f(1.f);

    m_TransformationMatrix = translate(m_TransformationMatrix, GetPosition());
    m_TransformationMatrix *= toMat4(GetRotation());
    m_TransformationMatrix = scale(m_TransformationMatrix, GetScale());
}

void Transform::OnRender(float deltaTime)
{
    CalculateTransformationMatrix();
}

void Transform::LoadData(const nlohmann::json &j)
{
    Serialization::LoadVector3(j, "pos", LocalPosition);
    Serialization::LoadQuaternion(j, "rot", LocalRotation);
    Serialization::LoadVector3(j, "scl", LocalScale);
}

void Transform::SaveData(nlohmann::json &j)
{
    j["pos"] = Serialization::StoreVector3(LocalPosition);
    j["rot"] = Serialization::StoreQuaternion(LocalRotation);
    j["scl"] = Serialization::StoreVector3(LocalScale);
}

Vector3f Transform::GetPosition() const
{
    Vector3f position = LocalPosition;

    if (m_Parent->GetParent() != nullptr)
    {
        position += m_Parent->GetParent()->GetTransform()->GetPosition();
    }

    return position;
}

Quaternion Transform::GetRotation() const
{
    Quaternion rotation = LocalRotation;

    if (m_Parent->GetParent() != nullptr)
    {
        rotation *= m_Parent->GetParent()->GetTransform()->GetRotation();
    }

    return rotation;
}

Vector3f Transform::GetScale() const
{
    Vector3f scale = LocalScale;

    if (m_Parent->GetParent() != nullptr)
    {
        scale += m_Parent->GetParent()->GetTransform()->GetScale();
    }

    return scale;
}

Vector3f Transform::GetForward() const
{
    return LocalRotation * Vector3f(0.f, 0.f, -1.f);
}

Vector3f Transform::GetRight() const
{
    return LocalRotation * Vector3f(1.f, 0.f, 0.f);
}

Vector3f Transform::GetUp() const
{
    return LocalRotation * Vector3f(0.f, 1.f, 0.f);
}

Vector3f Transform::GetEulerAngles() const
{
    return degrees(eulerAngles(LocalRotation));
}

void Transform::SetEulerAngles(Vector3f angle)
{
    LocalRotation = glm::quat(radians(angle));
}

const Matrix4f &Transform::GetTransformationMatrix() const
{
    return m_TransformationMatrix;
}
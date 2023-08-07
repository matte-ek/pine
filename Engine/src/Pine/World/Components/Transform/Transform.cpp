#include "Transform.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;

Pine::Transform::Transform() :
        IComponent(ComponentType::Transform)
{
}

void Pine::Transform::CalculateTransformationMatrix()
{
    m_TransformationMatrix = Matrix4f(1.f);

    m_TransformationMatrix = glm::translate(m_TransformationMatrix, GetPosition());
    m_TransformationMatrix *= glm::toMat4(GetRotation());
    m_TransformationMatrix = glm::scale(m_TransformationMatrix, GetScale());
}

void Pine::Transform::OnRender(float deltaTime)
{
    CalculateTransformationMatrix();
}

void Pine::Transform::LoadData(const nlohmann::json &j)
{
    Serialization::LoadVector3(j, "pos", LocalPosition);
    Serialization::LoadQuaternion(j, "rot", LocalRotation);
    Serialization::LoadVector3(j, "scl", LocalScale);
}

void Pine::Transform::SaveData(nlohmann::json &j)
{
    j["pos"] = Serialization::StoreVector3(LocalPosition);
    j["rot"] = Serialization::StoreQuaternion(LocalRotation);
    j["scl"] = Serialization::StoreVector3(LocalScale);
}

Vector3f Pine::Transform::GetPosition() const
{
    Vector3f position = LocalPosition;

    if (m_Parent->GetParent() != nullptr)
    {
        position += m_Parent->GetParent()->GetTransform()->GetPosition();
    }

    return position;
}

Quaternion Pine::Transform::GetRotation() const
{
    Quaternion rotation = LocalRotation;

    if (m_Parent->GetParent() != nullptr)
    {
        rotation *= m_Parent->GetParent()->GetTransform()->GetRotation();
    }

    return rotation;
}

Vector3f Pine::Transform::GetScale() const
{
    Vector3f scale = LocalScale;

    if (m_Parent->GetParent() != nullptr)
    {
        scale += m_Parent->GetParent()->GetTransform()->GetScale();
    }

    return scale;
}

Vector3f Pine::Transform::GetForward() const
{
    return LocalRotation * Vector3f(0.f, 0.f, -1.f);
}

Vector3f Pine::Transform::GetRight() const
{
    return LocalRotation * Vector3f(1.f, 0.f, 0.f);
}

Vector3f Pine::Transform::GetUp() const
{
    return LocalRotation * Vector3f(0.f, 1.f, 0.f);
}

Vector3f Pine::Transform::GetEulerAngles() const
{
    return glm::degrees(glm::eulerAngles(LocalRotation));
}

void Pine::Transform::SetEulerAngles(Vector3f angle)
{
    LocalRotation = glm::quat(glm::radians(angle));
}

const Matrix4f &Transform::GetTransformationMatrix() const
{
    return m_TransformationMatrix;
}
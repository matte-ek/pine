#include "Camera.hpp"
#include "Pine/World/Entity/Entity.hpp"

Pine::Camera::Camera() :
      IComponent(ComponentType::Camera)
{
}

void Pine::Camera::BuildProjectionMatrix()
{

}

void Pine::Camera::BuildViewMatrix()
{
    const auto transform = m_Parent->GetTransform();

    const auto position = transform->GetPosition();
    const auto rotation = glm::normalize(transform->GetRotation());

    const auto direction = rotation * Vector3f(0.f, 0.f, -1.f);
    const auto up = rotation * Vector3f(0.f, 1.f, 0.f);

    m_ViewMatrix = glm::lookAt(position, position + direction, up);
}

void Pine::Camera::OnRender(float)
{
    BuildProjectionMatrix();
    BuildViewMatrix();
}

void Pine::Camera::LoadData(const nlohmann::json& j)
{
}

void Pine::Camera::SaveData(nlohmann::json& j)
{
}

const Pine::Matrix4f& Pine::Camera::GetProjectionMatrix() const
{
    return m_ProjectionMatrix;
}

const Pine::Matrix4f& Pine::Camera::GetViewMatrix() const
{
    return m_ViewMatrix;
}
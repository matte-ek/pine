#include "Camera.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

Pine::Camera::Camera() :
        IComponent(ComponentType::Camera)
{
}

void Pine::Camera::SetOrthographicSize(float size)
{
    m_OrthographicSize = size;
}

float Pine::Camera::GetOrthographicSize() const
{
    return m_OrthographicSize;
}

void Pine::Camera::BuildProjectionMatrix()
{
    auto renderingContext = Pine::RenderManager::GetCurrentRenderingContext();
    const float aspectRatio = renderingContext->Size.x / renderingContext->Size.y;

    if (m_CameraType == CameraType::Orthographic)
        m_ProjectionMatrix = glm::ortho(-m_OrthographicSize, m_OrthographicSize, -m_OrthographicSize,
                                        m_OrthographicSize, -1.f, 1.f);
    if (m_CameraType == CameraType::Perspective)
        m_ProjectionMatrix = glm::perspective(glm::radians(m_FieldOfView), aspectRatio, m_NearPlane, m_FarPlane);
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

void Pine::Camera::LoadData(const nlohmann::json &j)
{
}

void Pine::Camera::SaveData(nlohmann::json &j)
{
}

const Pine::Matrix4f &Pine::Camera::GetProjectionMatrix() const
{
    return m_ProjectionMatrix;
}

const Pine::Matrix4f &Pine::Camera::GetViewMatrix() const
{
    return m_ViewMatrix;
}

void Pine::Camera::SetNearPlane(float value)
{
    m_NearPlane = value;
}

float Pine::Camera::GetNearPlane() const
{
    return m_NearPlane;
}

void Pine::Camera::SetFarPlane(float value)
{
    m_FarPlane = value;
}

float Pine::Camera::GetFarPlane() const
{
    return m_FarPlane;
}

void Pine::Camera::SetFieldOfView(float value)
{
    m_FieldOfView = value;
}

float Pine::Camera::GetFieldOfView() const
{
    return m_FieldOfView;
}

void Pine::Camera::SetCameraType(Pine::CameraType type)
{
    m_CameraType = type;
}

Pine::CameraType Pine::Camera::GetCameraType() const
{
    return m_CameraType;
}

#include "Camera.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

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
    auto renderingContext = RenderManager::GetCurrentRenderingContext();

    float aspectRatio = m_OverrideAspectRatio;

    if (m_OverrideAspectRatio == 0.f)
        aspectRatio = renderingContext->Size.x / renderingContext->Size.y;

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
    const auto rotation = normalize(transform->LocalRotation);

    const auto direction = rotation * Vector3f(0.f, 0.f, -1.f);
    const auto up = rotation * Vector3f(0.f, 1.f, 0.f);

    m_ViewMatrix = lookAt(position, position + direction, up);
}

void Pine::Camera::OnRender(float)
{
    BuildProjectionMatrix();
    BuildViewMatrix();
}

void Pine::Camera::LoadData(const nlohmann::json &j)
{
    Serialization::LoadValue(j, "cameraType", m_CameraType);
    Serialization::LoadValue(j, "nearPlane", m_NearPlane);
    Serialization::LoadValue(j, "farPlane", m_FarPlane);
    Serialization::LoadValue(j, "fieldOfView", m_FieldOfView);
    Serialization::LoadValue(j, "orthographicSize", m_OrthographicSize);
}

void Pine::Camera::SaveData(nlohmann::json &j)
{
    j["cameraType"] = static_cast<int>(m_CameraType);
    j["nearPlane"] = m_NearPlane;
    j["farPlane"] = m_FarPlane;
    j["fieldOfView"] = m_FieldOfView;
    j["orthographicSize"] = m_OrthographicSize;
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

void Pine::Camera::SetCameraType(CameraType type)
{
    m_CameraType = type;
}

Pine::CameraType Pine::Camera::GetCameraType() const
{
    return m_CameraType;
}

void Pine::Camera::SetClearColor(Vector4f color)
{
    m_ClearColor = color;
}

const Pine::Vector4f &Pine::Camera::GetClearColor() const
{
    return m_ClearColor;
}

void Pine::Camera::SetOverrideAspectRatio(float value)
{
    m_OverrideAspectRatio = value;
}

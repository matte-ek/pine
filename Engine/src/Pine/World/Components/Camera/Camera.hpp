#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

namespace Pine
{

    enum class CameraType
    {
	    Perspective,
        Orthographic
    };

    class Camera : public IComponent
    {
    private:
        CameraType m_CameraType = CameraType::Perspective;

        float m_NearPlane = 1.f;
        float m_FarPlane = 100.f;
        float m_FieldOfView = 70.f;

        float m_OrthographicSize = 1.f;

    	Matrix4f m_ProjectionMatrix = Matrix4f(1.f);
        Matrix4f m_ViewMatrix = Matrix4f(1.f);

        void BuildProjectionMatrix();
        void BuildViewMatrix();
    public:
        explicit Camera();

        void SetCameraType(CameraType type);
        CameraType GetCameraType() const;

        void SetNearPlane(float value);
        float GetNearPlane() const;

        void SetFarPlane(float value);
        float GetFarPlane() const;

        void SetFieldOfView(float value);
        float GetFieldOfView() const;

        void SetOrthographicSize(float size);
        float GetOrthographicSize() const;

        void OnRender(float) override;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;

        const Matrix4f& GetProjectionMatrix() const;
        const Matrix4f& GetViewMatrix() const;
    };

}
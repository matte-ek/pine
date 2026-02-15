#pragma once

#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{

    enum class CameraType
    {
	    Perspective,
        Orthographic
    };

    class Camera final : public Component
    {
    private:
        CameraType m_CameraType = CameraType::Perspective;

        Vector4f m_ClearColor = Vector4f(0.f);

        float m_NearPlane = 0.01f;
        float m_FarPlane = 150.f;
        float m_FieldOfView = 70.f;

        float m_OrthographicSize = 1.f;

        float m_OverrideAspectRatio = 0.f;

    	Matrix4f m_ProjectionMatrix = Matrix4f(1.f);
        Matrix4f m_ViewMatrix = Matrix4f(1.f);

        void BuildProjectionMatrix();
        void BuildViewMatrix();

        struct CameraSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Type, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(NearPlane, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(FarPlane, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(FieldOfView, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(OrthographicSize, Serialization::DataType::Float32);
        };
    public:
        explicit Camera();

        void SetCameraType(CameraType type);
        CameraType GetCameraType() const;

        void SetOverrideAspectRatio(float value);

        void SetNearPlane(float value);
        float GetNearPlane() const;

        void SetFarPlane(float value);
        float GetFarPlane() const;

        void SetFieldOfView(float value);
        float GetFieldOfView() const;

        void SetOrthographicSize(float size);
        float GetOrthographicSize() const;

        void SetClearColor(Vector4f color);
        const Vector4f& GetClearColor() const;

        Vector3f WorldToScreenPoint(const Vector3f& position) const;

        std::array<Vector3f, 8> GetFrustumCorners() const;

        void OnRender(float) override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;

        const Matrix4f& GetProjectionMatrix() const;
        const Matrix4f& GetViewMatrix() const;
    };

}
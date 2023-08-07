#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Pine
{

    enum class LightType
    {
        Directional,
        PointLight,
        SpotLight
    };

    class Light : public IComponent
    {
    private:
        LightType m_LightType = LightType::Directional;
        Vector3f m_LightColor = Vector3f(1.f, 1.f, 1.f);
    public:
        Light();

        void SetLightType(LightType type);
        LightType GetLightType() const;

        void SetLightColor(Vector3f color);
        const Vector3f& GetLightColor() const;
    };

}

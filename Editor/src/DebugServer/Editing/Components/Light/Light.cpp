#include "Light.hpp"

#include "../../Values/Values.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/Components/Light/Light.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;

    // These names are the wire representation; the numeric enum is an engine detail.
    const char* TypeName(const Pine::LightType type)
    {
        switch (type)
        {
        case Pine::LightType::Directional: return "Directional";
        case Pine::LightType::PointLight: return "PointLight";
        case Pine::LightType::SpotLight: return "SpotLight";
        }
        return "Unknown";
    }

    json Read(const Pine::Component* component)
    {
        const Pine::Light defaults;
        const auto light = component == nullptr ? &defaults : static_cast<const Pine::Light*>(component);
        return {
            { "Type", TypeName(light->GetLightType()) },
            { "Color", Pine::SerializationJson::StoreVector3(light->GetLightColor()) },
            { "Intensity", light->GetLightIntensity() },
            { "Range", light->GetRange() },
            { "CastShadows", light->GetCastShadows() },
            { "SpotlightOuterAngle", light->GetSpotlightOuterAngle() },
            { "SpotlightInnerAngle", light->GetSpotlightInnerAngle() }
        };
    }

    void Validate(const json& state, const std::string& path)
    {
        Values::Require(state.at("SpotlightInnerAngle").get<float>() <= state.at("SpotlightOuterAngle").get<float>(),
            path + "/SpotlightInnerAngle", "Inner angle must not exceed outer angle.");
    }

    void Apply(Pine::Component* component, const json& state)
    {
        const auto light = static_cast<Pine::Light*>(component);
        const auto name = state.at("Type").get<std::string>();
        auto type = Pine::LightType::Directional;
        if (name == "PointLight")
        {
            type = Pine::LightType::PointLight;
        }
        else if (name == "SpotLight")
        {
            type = Pine::LightType::SpotLight;
        }

        light->SetLightType(type);
        light->SetLightColor(Values::Vector3(state.at("Color")));
        light->SetLightIntensity(state.at("Intensity").get<float>());
        light->SetRange(state.at("Range").get<float>());
        light->SetCastShadows(state.at("CastShadows").get<bool>());

        // The final pair was validated together. Outer first avoids clamping a new inner angle
        // against the old outer angle, regardless of the order of properties in the request.
        light->SetSpotlightOuterAngle(state.at("SpotlightOuterAngle").get<float>());
        light->SetSpotlightInnerAngle(state.at("SpotlightInnerAngle").get<float>());
    }
}

const Editor::DebugServer::Editing::Components::Adapter&
Editor::DebugServer::Editing::Components::Light::GetAdapter()
{
    static const Adapter adapter = {
        Pine::ComponentType::Light, "Light",
        {
            { "Type", { { "type", "enum" }, { "values", { "Directional", "PointLight", "SpotLight" } } } },
            { "Color", { { "type", "vector3" }, { "minimum", 0 }, { "colorSpace", "linear" } } },
            { "Intensity", { { "type", "number" }, { "minimum", 0 } } },
            { "Range", { { "type", "number" }, { "minimum", 0.01f }, { "units", "world units" } } },
            { "CastShadows", { { "type", "boolean" } } },
            { "SpotlightOuterAngle", { { "type", "number" }, { "minimum", 1 }, { "maximum", 89 }, { "units", "degrees" } } },
            { "SpotlightInnerAngle", { { "type", "number" }, { "minimum", 0 }, { "maximum", 89 },
                { "units", "degrees" }, { "constraint", "Must not exceed SpotlightOuterAngle" } } }
        },
        Read, Validate, Apply, true
    };
    return adapter;
}

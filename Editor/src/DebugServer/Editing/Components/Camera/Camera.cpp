#include "Camera.hpp"

#include <cmath>

#include "../../Values/Values.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;

    json Read(const Pine::Component* component)
    {
        const Pine::Camera defaults;
        const auto camera = component == nullptr ? &defaults : static_cast<const Pine::Camera*>(component);
        return {
            { "Type", camera->GetCameraType() == Pine::CameraType::Perspective ? "Perspective" : "Orthographic" },
            { "FieldOfView", camera->GetFieldOfView() },
            { "NearPlane", camera->GetNearPlane() },
            { "FarPlane", camera->GetFarPlane() }
        };
    }

    void Validate(const json& state, const std::string& path)
    {
        // Compare float32 values, since distinct JSON numbers can round to the same plane.
        Values::Require(state.at("NearPlane").get<float>() > 0.f,
            path + "/NearPlane", "Near plane must remain positive as float32.");
        Values::Require(state.at("FarPlane").get<float>() > state.at("NearPlane").get<float>(),
            path + "/FarPlane", "Far plane must exceed near plane as float32.");

        // Clipping terms can overflow even when each supplied distance is finite.
        const auto projection = glm::perspective(glm::radians(state.at("FieldOfView").get<float>()),
            1.f, state.at("NearPlane").get<float>(), state.at("FarPlane").get<float>());
        const auto inverseProjection = glm::inverse(projection);
        for (int column = 0; column < 4; column++)
        {
            for (int row = 0; row < 4; row++)
            {
                Values::Require(std::isfinite(projection[column][row]) && std::isfinite(inverseProjection[column][row]),
                    path, "Camera parameters must produce finite projection and inverse projection matrices.");
            }
        }
    }

    void Apply(Pine::Component* component, const json& state)
    {
        const auto camera = static_cast<Pine::Camera*>(component);
        camera->SetCameraType(Pine::CameraType::Perspective);
        camera->SetFieldOfView(state.at("FieldOfView").get<float>());
        camera->SetNearPlane(state.at("NearPlane").get<float>());
        camera->SetFarPlane(state.at("FarPlane").get<float>());
    }
}

const Editor::DebugServer::Editing::Components::Adapter&
Editor::DebugServer::Editing::Components::Camera::GetAdapter()
{
    static const Adapter adapter = {
        Pine::ComponentType::Camera, "Camera",
        {
            { "Type", { { "type", "enum" }, { "values", { "Perspective" } } } },
            { "FieldOfView", { { "type", "number" }, { "minimum", 1 }, { "maximum", 179 },
                { "units", "degrees" }, { "description", "Vertical field of view; aspect ratio comes from the viewport." } } },
            { "NearPlane", { { "type", "number" }, { "minimum", 0 }, { "units", "world units" },
                { "constraint", "Must be positive as float32 and less than FarPlane" } } },
            { "FarPlane", { { "type", "number" }, { "minimum", 0 }, { "units", "world units" },
                { "constraint", "Must exceed NearPlane as float32" } } }
        },
        Read, Validate, Apply, true
    };
    return adapter;
}

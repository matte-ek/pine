#include "RigidBody.hpp"

#include <cmath>

#include "../../Values/Values.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;

    const char* TypeName(const Pine::RigidBodyType type)
    {
        switch (type)
        {
        case Pine::RigidBodyType::Static: return "Static";
        case Pine::RigidBodyType::Kinematic: return "Kinematic";
        case Pine::RigidBodyType::Dynamic: return "Dynamic";
        }
        return "Unknown";
    }

    json Locks(const std::array<bool, 3>& locks)
    {
        return { { "x", locks[0] }, { "y", locks[1] }, { "z", locks[2] } };
    }

    std::array<bool, 3> Locks(const json& locks)
    {
        return { locks.at("x").get<bool>(), locks.at("y").get<bool>(), locks.at("z").get<bool>() };
    }

    json Read(const Pine::Component* component)
    {
        const Pine::RigidBody defaults;
        const auto body = component == nullptr ? &defaults : static_cast<const Pine::RigidBody*>(component);
        return {
            { "Type", TypeName(body->GetRigidBodyType()) },
            { "Mass", body->GetMass() },
            { "GravityEnabled", body->GetGravityEnabled() },
            { "PositionLock", Locks(body->GetPositionLock()) },
            { "RotationLock", Locks(body->GetRotationLock()) },
            { "MaxLinearVelocity", body->GetMaxLinearVelocity() },
            { "MaxAngularVelocity", body->GetMaxAngularVelocity() }
        };
    }

    void Validate(const json& state, const std::string& path)
    {
        const auto mass = state.at("Mass").get<float>();
        Values::Require(mass > 0.f, path + "/Mass", "Mass must remain positive as float32.");
        // PhysX stores inverse mass. A positive subnormal can still overflow that value.
        Values::Require(std::isfinite(1.f / mass), path + "/Mass", "Inverse mass must remain finite as float32.");
    }

    void Apply(Pine::Component* component, const json& state)
    {
        const auto body = static_cast<Pine::RigidBody*>(component);
        const auto name = state.at("Type").get<std::string>();
        auto type = Pine::RigidBodyType::Dynamic;
        if (name == "Static")
        {
            type = Pine::RigidBodyType::Static;
        }
        else if (name == "Kinematic")
        {
            type = Pine::RigidBodyType::Kinematic;
        }

        body->SetRigidBodyType(type);
        body->SetMass(state.at("Mass").get<float>());
        body->SetGravityEnabled(state.at("GravityEnabled").get<bool>());
        body->SetPositionLock(Locks(state.at("PositionLock")));
        body->SetRotationLock(Locks(state.at("RotationLock")));
        body->SetMaxLinearVelocity(state.at("MaxLinearVelocity").get<float>());
        body->SetMaxAngularVelocity(state.at("MaxAngularVelocity").get<float>());
    }
}

const Editor::DebugServer::Editing::Components::Adapter&
Editor::DebugServer::Editing::Components::RigidBody::GetAdapter()
{
    static const Adapter adapter = {
        Pine::ComponentType::RigidBody, "RigidBody",
        {
            { "Type", { { "type", "enum" }, { "values", { "Static", "Kinematic", "Dynamic" } },
                { "description", "Entity static=true forces a static actor. A Collider on the same entity is needed for an actor; a body without one remains dormant." } } },
            { "Mass", { { "type", "number" }, { "minimum", 0 }, { "units", "kilograms" },
                { "constraint", "Must be positive with a finite reciprocal as float32; retained but unused by static actors." } } },
            { "GravityEnabled", { { "type", "boolean" } } },
            { "PositionLock", { { "type", "boolean3" }, { "space", "world axes" } } },
            { "RotationLock", { { "type", "boolean3" }, { "space", "world axes" } } },
            { "MaxLinearVelocity", { { "type", "number" }, { "minimum", 0 }, { "units", "world units per second" },
                { "description", "Zero uses the physics backend default on creation." } } },
            { "MaxAngularVelocity", { { "type", "number" }, { "minimum", 0 }, { "units", "radians per second" },
                { "description", "Zero uses the physics backend default on creation." } } }
        },
        Read, Validate, Apply, true
    };
    return adapter;
}

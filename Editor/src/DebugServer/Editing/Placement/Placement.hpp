#pragma once

#include "../Duplication/Duplication.hpp"

namespace Editor::DebugServer::Editing::Placement
{
    struct WorldTransform
    {
        Pine::Vector3f Position = Pine::Vector3f(0.f);
        Pine::Quaternion Rotation = Pine::Quaternion(1.f, 0.f, 0.f, 0.f);
        Pine::Vector3f Scale = Pine::Vector3f(1.f);
    };

    nlohmann::json Fields();
    void Validate(nlohmann::json& input, const std::string& path);
    nlohmann::json AimFields();
    void ValidateAim(nlohmann::json& input, const std::string& path);
    nlohmann::json ColliderFitFields();
    void ValidateColliderFit(nlohmann::json& input, const std::string& path);
    WorldTransform Compose(const WorldTransform& parent, const nlohmann::json& local, const std::string& path);

    // Computes a Transform adapter state without changing live entities or cached bounds.
    nlohmann::json Prepare(const nlohmann::json& input, const Duplication::EntityState& entity,
        const WorldTransform& parent, const std::string& path);
    nlohmann::json PrepareAim(const nlohmann::json& input, const Duplication::EntityState& entity,
        const WorldTransform& parent, const std::string& path);
    nlohmann::json PrepareRelative(const nlohmann::json& input, const Duplication::EntityState& entity,
        const WorldTransform& parent, const Duplication::EntityState& reference,
        const WorldTransform& referenceParent, const std::string& path);

    // Computes a Collider adapter state whose box wraps the entity's own model geometry.
    nlohmann::json PrepareColliderFit(const nlohmann::json& input, const Duplication::EntityState& entity,
        const WorldTransform& parent, const std::string& path);
}

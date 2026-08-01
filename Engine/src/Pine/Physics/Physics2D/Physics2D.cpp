#include "Physics2D.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Collider2D/Collider2D.hpp"
#include "Pine/World/Components/RigidBody2D/RigidBody2D.hpp"

namespace
{
    b2World *m_World = nullptr;
}

void Pine::Physics2D::Setup()
{
}

void Pine::Physics2D::Shutdown()
{
}

void Pine::Physics2D::Update(const double deltaTime)
{
    PINE_PF_SCOPE();

    static double accumulator = 0.0;

    if (World::IsPaused())
    {
        accumulator = 0.0;
        return;
    }

    constexpr float timeStep = 1.0 / 120.0;

    accumulator += deltaTime;

    if (accumulator <= timeStep)
        return;

    const double physicsTimeDelta = accumulator;

    accumulator = 0.0;

    for (auto& collider : Pine::Components::Get<Collider2D>())
        collider.OnPrePhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<RigidBody2D>())
        rigidBody.OnPrePhysicsUpdate();

    for (auto& collider : Pine::Components::Get<Collider2D>())
        collider.OnPostPhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<RigidBody2D>())
        rigidBody.OnPostPhysicsUpdate();
}

b2World * Pine::Physics2D::GetWorld()
{
    return m_World;
}
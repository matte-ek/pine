#include "Physics2D.hpp"
#include "box2d/b2_world.h"
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
    b2Vec2 gravity(0.0f, -9.81f);

    m_World = new b2World(gravity);
}

void Pine::Physics2D::Shutdown()
{
	delete m_World;
}

void Pine::Physics2D::Update(double deltaTime)
{
    static double accumulator = 0.0;

    if (Pine::World::IsPaused())
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

    for (auto& collider : Pine::Components::Get<Pine::Collider2D>())
        collider.OnPrePhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<Pine::RigidBody2D>())
        rigidBody.OnPrePhysicsUpdate();

    m_World->Step(physicsTimeDelta, 8, 3);

    for (auto& collider : Pine::Components::Get<Pine::Collider2D>())
        collider.OnPostPhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<Pine::RigidBody2D>())
        rigidBody.OnPostPhysicsUpdate();
}

b2World * Pine::Physics2D::GetWorld()
{
    return m_World;
}
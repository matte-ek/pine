#include "Physics3D.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"

namespace
{
    reactphysics3d::PhysicsCommon *m_PhysicsCommon = nullptr;
    reactphysics3d::PhysicsWorld *m_PhysicsWorld = nullptr;
} // namespace

void Pine::Physics3D::Setup()
{
    m_PhysicsCommon = new reactphysics3d::PhysicsCommon;

    reactphysics3d::PhysicsWorld::WorldSettings settings;

    settings.defaultVelocitySolverNbIterations = 20;
    settings.isSleepingEnabled = false;
    settings.gravity = reactphysics3d::Vector3(0, -9.81, 0);
    settings.isSleepingEnabled = true;

    m_PhysicsWorld = m_PhysicsCommon->createPhysicsWorld(settings);
    m_PhysicsWorld->setIsDebugRenderingEnabled(true);
    m_PhysicsWorld->enableSleeping(true);
}

void Pine::Physics3D::Shutdown()
{
    delete m_PhysicsCommon;
}

void Pine::Physics3D::Update(double deltaTime)
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

    for (auto& collider : Pine::Components::Get<Pine::Collider>())
        collider.OnPrePhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<Pine::RigidBody>())
        rigidBody.OnPrePhysicsUpdate();

    m_PhysicsWorld->update(physicsTimeDelta);

    for (auto& collider : Pine::Components::Get<Pine::Collider>())
        collider.OnPostPhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<Pine::RigidBody>())
        rigidBody.OnPostPhysicsUpdate();
}

reactphysics3d::PhysicsCommon *Pine::Physics3D::GetCommon()
{
    return m_PhysicsCommon;
}

reactphysics3d::PhysicsWorld *Pine::Physics3D::GetWorld()
{
    return m_PhysicsWorld;
}

void Pine::Physics3D::RenderDebugColliders()
{
    reactphysics3d::DebugRenderer& debugRenderer = m_PhysicsWorld->getDebugRenderer();



}

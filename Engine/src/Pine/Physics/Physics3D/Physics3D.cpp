#include "Physics3D.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"

#include "physx/PxPhysicsAPI.h"

namespace
{
    physx::PxDefaultAllocator m_Allocator;
    physx::PxDefaultErrorCallback m_ErrorCallback;

    physx::PxFoundation* m_Foundation;

    physx::PxPhysics* m_Physics;

    physx::PxDefaultCpuDispatcher* m_Dispatcher;

    physx::PxScene* m_Scene;

    physx::PxMaterial* m_DefaultMaterial;
}

void Pine::Physics3D::Setup()
{
    m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
    m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, physx::PxTolerancesScale(), true, nullptr);

    m_Dispatcher = physx::PxDefaultCpuDispatcherCreate(2);

    m_DefaultMaterial = m_Physics->createMaterial(1, 1, 1);

    physx::PxSceneDesc sceneDescriptor(m_Physics->getTolerancesScale());

    sceneDescriptor.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
    sceneDescriptor.cpuDispatcher = m_Dispatcher;
    sceneDescriptor.filterShader = physx::PxDefaultSimulationFilterShader;

    m_Scene = m_Physics->createScene(sceneDescriptor);
}

void Pine::Physics3D::Shutdown()
{
    PX_RELEASE(m_Scene);
    PX_RELEASE(m_Dispatcher);
    PX_RELEASE(m_Physics);
    PX_RELEASE(m_Foundation);
}

void Pine::Physics3D::Update(double deltaTime)
{
    static double accumulator = 0.0;

    if (World::IsPaused())
    {
        accumulator = 0.0;
        return;
    }

    return;

    constexpr float timeStep = 1.0 / 120.0;

    accumulator += deltaTime;

    if (accumulator <= timeStep)
        return;

    const auto physicsTimeDelta = static_cast<float>(accumulator);

    accumulator = 0.0;

    for (auto& collider : Pine::Components::Get<Collider>())
        collider.OnPrePhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<RigidBody>())
        rigidBody.OnPrePhysicsUpdate();

    m_Scene->simulate(physicsTimeDelta);

    for (auto& collider : Pine::Components::Get<Collider>())
        collider.OnPostPhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<RigidBody>())
        rigidBody.OnPostPhysicsUpdate();
}

physx::PxPhysics* Pine::Physics3D::GetPhysics()
{
    return m_Physics;
}

physx::PxScene * Pine::Physics3D::GetScene()
{
    return m_Scene;
}

physx::PxMaterial * Pine::Physics3D::GetDefaultMaterial()
{
    return m_DefaultMaterial;
}
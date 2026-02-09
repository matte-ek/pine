#include "Physics3D.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"



#include "physx/PxPhysicsAPI.h"
#include "Pine/Performance/Performance.hpp"

namespace
{
    using namespace physx;

    PxDefaultAllocator m_Allocator;
    PxDefaultErrorCallback m_ErrorCallback;

    PxFoundation* m_Foundation;

    PxPhysics* m_Physics;

    PxDefaultCpuDispatcher* m_Dispatcher;

    PxScene* m_Scene;

    PxMaterial* m_DefaultMaterial;

    PxPvd* m_Pvd;

    PxFilterFlags PineFilterShader(
        PxFilterObjectAttributes attributes0, PxFilterData filterData0,
        PxFilterObjectAttributes attributes1, PxFilterData filterData1,
        PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
    {
        if(PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
        {
            // Handle the trigger layer mask, word3.
            if ((filterData0.word3 & filterData1.word0) || (filterData1.word3 & filterData0.word0))
            {
                pairFlags |= PxPairFlag::eTRIGGER_DEFAULT;
            }

            return PxFilterFlag::eDEFAULT;
        }

        // Make sure the layer mask of each object allow these objects to collide.
        if ((filterData0.word0 & filterData1.word1) && (filterData1.word0 & filterData0.word1))
        {
            pairFlags = PxPairFlag::eCONTACT_DEFAULT;
        }

        return PxFilterFlag::eDEFAULT;
    }
}

void Pine::Physics3D::Setup()
{
    m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
    m_Pvd = PxCreatePvd(*m_Foundation);

    m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, PxTolerancesScale(), true, m_Pvd);

    m_Dispatcher = PxDefaultCpuDispatcherCreate(2);

    m_DefaultMaterial = m_Physics->createMaterial(0.5f, 0.5f, 0.1f);

    PxSceneDesc sceneDescriptor(m_Physics->getTolerancesScale());

    sceneDescriptor.gravity = PxVec3(0.0f, -9.81f, 0.0f);
    sceneDescriptor.cpuDispatcher = m_Dispatcher;
    sceneDescriptor.filterShader = PineFilterShader;

    m_Scene = m_Physics->createScene(sceneDescriptor);
}

void Pine::Physics3D::Shutdown()
{
    PX_RELEASE(m_Scene);
    PX_RELEASE(m_Dispatcher);
    PX_RELEASE(m_Physics);
    PX_RELEASE(m_Foundation);
}

void Pine::Physics3D::ConnectVisualDebugger()
{
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);

    m_Pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

    if (PxPvdSceneClient* pvdClient = m_Scene->getScenePvdClient())
    {
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }
}

void Pine::Physics3D::Update(double deltaTime)
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

    const auto physicsTimeDelta = static_cast<float>(accumulator);

    accumulator = 0.0;

    for (auto& collider : Pine::Components::Get<Collider>())
        collider.OnPrePhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<RigidBody>())
        rigidBody.OnPrePhysicsUpdate();

    m_Scene->simulate(physicsTimeDelta);
    m_Scene->fetchResults(true);

    for (auto& collider : Pine::Components::Get<Collider>())
        collider.OnPostPhysicsUpdate();
    for (auto& rigidBody : Pine::Components::Get<RigidBody>())
        rigidBody.OnPostPhysicsUpdate();
}

PxPhysics* Pine::Physics3D::GetPhysics()
{
    return m_Physics;
}

PxScene * Pine::Physics3D::GetScene()
{
    return m_Scene;
}

PxMaterial * Pine::Physics3D::GetDefaultMaterial()
{
    return m_DefaultMaterial;
}
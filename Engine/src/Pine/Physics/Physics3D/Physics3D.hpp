#pragma once
#include <cstdint>
#include <reactphysics3d/reactphysics3d.h>

namespace Pine::Physics3D
{

    void Setup();
    void Shutdown();

    void Update(double deltaTime);

    reactphysics3d::PhysicsCommon* GetCommon();
    reactphysics3d::PhysicsWorld* GetWorld();

}
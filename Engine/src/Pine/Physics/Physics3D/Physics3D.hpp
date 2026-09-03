#pragma once

namespace physx
{
    class PxPhysics;
    class PxScene;
    class PxMaterial;
    class PxControllerManager;
}

namespace Pine::Physics3D
{

    void Setup();
    void Shutdown();

    void ConnectVisualDebugger();

    void Update(double deltaTime);

    physx::PxPhysics* GetPhysics();
    physx::PxScene* GetScene();
    physx::PxMaterial* GetDefaultMaterial();
    physx::PxControllerManager* GetControllerManager();

}
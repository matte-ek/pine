#pragma once

namespace physx
{
    class PxMaterial;
    class PxShape;
}

namespace Pine
{
    class Terrain;
}

namespace Pine::Physics3D::TerrainCollision
{
    // Cooks a terrain's height field into a PhysX collision shape, or null when there is nothing to
    // cook or PhysX refuses the field.
    //
    // One height field for the whole terrain rather than one per chunk. Chunks exist for rendering
    // LOD and frustum culling; collision needs neither, and PhysX runs its own broadphase over a
    // height field. Keeping it whole is what lets a terrain attach as a *single* shape, so neither
    // Collider nor RigidBody has to learn about multi-shape actors, and it reduces the shape's
    // local pose to one offset instead of per-chunk arithmetic.
    //
    // The returned shape holds the only reference to the cooked field, so releasing the shape is
    // what frees it - there is no second handle to keep, and none to forget.
    //
    // The trade is that any edit re-cooks the whole field. At the sizes a terrain is built for that
    // is a millisecond, but it is far too slow to run per frame: rebuild on the end of a sculpt
    // stroke or on save, which for a Collider means calling Reset() and letting the next physics
    // update build a new actor.
    physx::PxShape* CreateShape(const Terrain& terrain, physx::PxMaterial& material);
}

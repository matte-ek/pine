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
    // is a millisecond, but it is far too slow to run per frame, so a height field is only rebuilt
    // once an edit has finished - see RebuildColliders.
    physx::PxShape* CreateShape(const Terrain& terrain, physx::PxMaterial& material);

    // Resets every height field Collider on an entity rendering this terrain, so the next physics
    // update cooks the terrain's current heights. Call it once an edit to the heights or layout is
    // complete - at the end of a sculpt stroke, not on each step of one.
    void RebuildColliders(const Terrain& terrain);
}

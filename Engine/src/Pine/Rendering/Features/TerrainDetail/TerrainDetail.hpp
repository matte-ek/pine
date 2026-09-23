#pragma once

#include "Pine/Rendering/RenderingContext.hpp"

namespace Pine::Rendering::TerrainDetail
{
    // Draws the detail types of every terrain in the world - see TerrainDetailType. Placements
    // are generated per terrain chunk and detail type, uploaded once into a storage buffer and
    // kept, so a frame costs one draw per (chunk, detail type, mesh) rather than any per-instance
    // work on the CPU.
    //
    // Only chunks near a camera hold placements: they are generated when a chunk comes within a
    // detail type's draw distance of any camera and released once it is well past it, so the
    // memory held follows what is around the viewers rather than the size of the terrain.

    // Releases every placement buffer.
    void Shutdown();

    // Brings the placements up to date with the cameras and the terrains: generates what has come
    // into range or changed since it was generated, and releases what nothing is near any more.
    // Call once per frame, after TerrainRenderer::Prepare, which assigns the chunk light slots the
    // detail is lit through.
    void Prepare();

    // Draws the detail this context's camera can see. Scene pass only: detail neither writes the
    // depth pre-pass nor casts shadows, the same as a Discard material in the pre-pass.
    void Render(RenderingContext& context);
}

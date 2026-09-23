#pragma once
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Math/Frustum/Frustum.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"

namespace Pine
{

    struct RenderingStatistics
    {
        int LightCount = 0;
        int ModelLightCalculationCount = 0;
        int DrawCalls = 0;

        // Vertices submitted, which for an indexed draw is its index count and for an instanced one
        // is that times the instance count - what the draw call actually asks the GPU to process,
        // rather than how many unique vertices the meshes hold. It is the number terrain LOD moves.
        std::uint64_t VertexCount = 0;
        double RenderTime = 0.f;

        int VisibleObjectCount = 0;
        int CulledObjectCount = 0;

        // Terrain chunks are counted separately because they are not objects in the sense the two
        // counters above mean: a whole terrain is one component, and the counts below are of the
        // chunks under it. Adding them into the object counts would make "visible objects" mean
        // something different for a level that has a terrain in it.
        int VisibleTerrainChunkCount = 0;
        int CulledTerrainChunkCount = 0;

        // Terrain detail copies submitted by the scene pass, across every chunk and detail type. The
        // draws and vertices they cost are in the counters above as well.
        std::uint64_t TerrainDetailInstanceCount = 0;

        void Reset()
        {
            LightCount = 0;
            ModelLightCalculationCount = 0;
            DrawCalls = 0;
            VertexCount = 0;
            RenderTime = 0.f;
            VisibleObjectCount = 0;
            CulledObjectCount = 0;
            VisibleTerrainChunkCount = 0;
            CulledTerrainChunkCount = 0;
            TerrainDetailInstanceCount = 0;
        }
    };

    struct RenderingContext
    {
        bool Active = true;
        bool UseRenderPipeline = true;

        Vector2f Size = Vector2f(0.f);

        Camera* SceneCamera = nullptr;

        Graphics::IFrameBuffer* FrameBuffer = nullptr;

        Vector4f ClearColor = Vector4f(0.f, 0.f, 0.f, 1.f);

        Texture3D* Skybox = nullptr;

        bool EnableStencilBuffer = true;

        RenderingStatistics Statistics;

        // The volume SceneCamera can see, rebuilt once per frame in the pipeline's prepass and read
        // by both stages after it. Owned per context for the same reason the visibility set below
        // is: two viewports looking different ways have different frustums.
        //
        // It exists as well as that set because not everything culls per component. A terrain has
        // many chunks under one component id and so cannot be represented in a set indexed by that
        // id; it tests its chunks against this frustum inline instead.
        Frustum ViewFrustum;

        // What this context's camera can see. Owned per context because visibility depends on the
        // frustum: two viewports looking different ways cull differently, and both stages of this
        // context's frame read the same set.
        Rendering::RenderCulling::VisibilitySet Visibility;

        int PreAllocItems = 0;
    };

}
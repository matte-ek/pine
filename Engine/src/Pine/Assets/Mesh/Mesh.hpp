#pragma once

#include <vector>

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Graphics/Interfaces/IVertexArray.hpp"

namespace Pine
{
    class Model;

    class Mesh
    {
    private:
        Graphics::IVertexArray* m_VertexArray = nullptr;

        // The buffers the Set* calls below created, kept so that a mesh whose layout has not
        // changed can have its values re-uploaded rather than be rebuilt. Rebuilding allocates a
        // fresh buffer on the vertex array every time, and the array only frees its buffers when it
        // is disposed - so a mesh rewritten every frame, which is what sculpting a terrain chunk
        // does, would leak a set of them per rewrite.
        Graphics::IVertexBuffer* m_VertexBuffer = nullptr;
        Graphics::IVertexBuffer* m_NormalBuffer = nullptr;
        Graphics::IVertexBuffer* m_TangentBuffer = nullptr;
        Graphics::IVertexBuffer* m_UvBuffer = nullptr;

        // Default material
        AssetHandle<Material> m_Material = UId("272acbdf9d24-938e65cf3a4b101d");

        std::uint32_t m_RenderCount = 0;
        std::uint32_t m_VertexCount = 0;
        bool m_HasElementBuffer = false;

        Model* m_Model = nullptr;

        bool m_HasTangentData = false;

        Vector3f m_BoundingBoxMin = {};
        Vector3f m_BoundingBoxMax = {};

        // Shared by the four Update* calls below. Named so that the warning it produces says which
        // attribute was refused, which is the only thing that distinguishes one call from another.
        static void UploadAttribute(Graphics::IVertexBuffer* buffer,
                                    const char* name,
                                    const void* data,
                                    std::size_t size,
                                    std::size_t offset);
    public:
        explicit Mesh(Model* model);

        Graphics::IVertexArray* GetVertexArray() const;

        std::uint32_t GetRenderCount() const;

        // How many vertices SetVertices was given. Separate from GetRenderCount, which becomes the
        // index count as soon as the mesh has an element buffer - so it is this, not that, which
        // tells a rebuild whether the mesh it is about to write has the layout it already has.
        std::uint32_t GetVertexCount() const;

        bool HasElementBuffer() const;

        // Reads the positions and indices back off the GPU, so what comes back includes whatever
        // UpdateVertices last wrote. The mesh keeps no copy of its own, which is why this has to
        // ask the graphics API for them: it needs the graphics context, and it stalls the thread
        // until the readback lands. A mesh with no element buffer gives back an empty index
        // vector. A mesh whose geometry cannot be read gives back false and clears both vectors.
        bool ReadGeometry(std::vector<Vector3f>& vertices, std::vector<std::uint32_t>& indices) const;

        void SetMaterial(Material* material);
        void SetMaterial(UId id);

        Material* GetMaterial() const;

        // The material this mesh names, whether or not it is loaded. What a save writes, so a
        // material that is missing right now is still referenced once it comes back.
        const UId& GetMaterialUId() const;

        Model* GetModel() const;

        const Vector3f& GetBoundingBoxMin() const;
        const Vector3f& GetBoundingBoxMax() const;

        // Each one creates the attribute's buffer and fills it. The usage hint says how the buffer
        // is expected to be written afterwards: the default suits a mesh loaded from a model and
        // then left alone, and a caller that will keep calling the matching Update* below - terrain
        // chunks do, once per frame while a brush is dragged - passes DynamicDraw instead.
        void SetVertices(float* vertices, std::size_t size, Graphics::BufferUsageHint usage = Graphics::BufferUsageHint::StaticDraw);
        void SetIndices(std::uint32_t* indices, std::size_t size);
        void SetNormals(float* normals, std::size_t size, Graphics::BufferUsageHint usage = Graphics::BufferUsageHint::StaticDraw);
        void SetTangents(float* tangents, std::size_t size, Graphics::BufferUsageHint usage = Graphics::BufferUsageHint::StaticDraw);
        void SetUvs(float* uvs, std::size_t size, Graphics::BufferUsageHint usage = Graphics::BufferUsageHint::StaticDraw);

        // Overwrite part or all of a buffer one of the Set* calls already created, leaving the
        // buffer and its binding on the vertex array alone. The data has to describe the same
        // vertices in the same order - these cannot grow a buffer or change its layout.
        //
        // Writing past the end of the buffer, or updating an attribute that was never set, warns
        // and changes nothing.
        void UpdateVertices(const float* vertices, std::size_t size, std::size_t offset = 0);
        void UpdateNormals(const float* normals, std::size_t size, std::size_t offset = 0);
        void UpdateTangents(const float* tangents, std::size_t size, std::size_t offset = 0);
        void UpdateUvs(const float* uvs, std::size_t size, std::size_t offset = 0);

        void SetAABB(Vector3f min, Vector3f max);

        void Dispose();
    };
}

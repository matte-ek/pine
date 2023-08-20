#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Graphics/Interfaces/IVertexArray.hpp"

namespace Pine
{
    class Model;

    class Mesh
    {
    private:
        Graphics::IVertexArray* m_VertexArray = nullptr;
        AssetContainer<Material> m_Material;

        std::uint32_t m_RenderCount = 0;
        bool m_HasElementBuffer = false;

        Model* m_Model = nullptr;

        bool m_HasTangentData = false;
    public:
        explicit Mesh(Model* model);

        Graphics::IVertexArray* GetVertexArray() const;

        std::uint32_t GetRenderCount() const;
        bool HasElementBuffer() const;

        void SetMaterial(Material* material);
        void SetMaterial(const std::string& fileReference);

        Material* GetMaterial() const;

        Model* GetModel() const;

        void SetVertices(float* vertices, std::size_t size);
        void SetIndices(std::uint32_t* indices, std::size_t size);
        void SetNormals(float* normals, std::size_t size);
        void SetTangents(float* tangents, std::size_t size);
        void SetUvs(float* uvs, std::size_t size);

        void Dispose();
    };
}
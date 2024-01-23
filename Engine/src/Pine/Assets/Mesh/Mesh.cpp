#include "Mesh.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"

using namespace Pine::Renderer3D::Specifications;

Pine::Mesh::Mesh(Model*model)
{
    m_Model = model;
    m_VertexArray = Graphics::GetGraphicsAPI()->CreateVertexArray();
    m_Material = Assets::Get<Material>("engine/materials/default.mat");
}

void Pine::Mesh::Dispose()
{
    m_VertexArray->Dispose();

    Graphics::GetGraphicsAPI()->DestroyVertexArray(m_VertexArray);

    m_VertexArray = nullptr;
}

Pine::Graphics::IVertexArray *Pine::Mesh::GetVertexArray() const
{
    return m_VertexArray;
}

std::uint32_t Pine::Mesh::GetRenderCount() const
{
    return m_RenderCount;
}

bool Pine::Mesh::HasElementBuffer() const
{
    return m_HasElementBuffer;
}

void Pine::Mesh::SetMaterial(Material*material)
{
    m_Material = material;
}

void Pine::Mesh::SetMaterial(const std::string &fileReference)
{
    assert(!fileReference.empty());
    assert(Pine::Assets::GetState() == AssetManagerState::LoadDirectory);

    Assets::AddAssetResolveReference({fileReference, reinterpret_cast<AssetHandle<IAsset>*>(&m_Material)});
}

Pine::Material *Pine::Mesh::GetMaterial() const
{
    return m_Material.Get();
}

Pine::Model *Pine::Mesh::GetModel() const
{
    return m_Model;
}

void Pine::Mesh::SetVertices(float* vertices, std::size_t size)
{
    m_VertexArray->Bind();
    m_VertexArray->StoreFloatArrayBuffer(reinterpret_cast<float*>(vertices), size, Buffers::VERTEX_ARRAY_BUFFER, 3, Graphics::BufferUsageHint::StaticDraw);
    m_RenderCount = static_cast<std::uint32_t>(size / sizeof(Vector3f));
}

void Pine::Mesh::SetIndices(std::uint32_t *indices, std::size_t size)
{
    m_VertexArray->Bind();
    m_VertexArray->StoreElementArrayBuffer(indices, size);
    m_HasElementBuffer = true;
    m_RenderCount = static_cast<std::uint32_t>(size / sizeof(std::uint32_t));
}

void Pine::Mesh::SetNormals(float* normals, std::size_t size)
{
    m_VertexArray->Bind();
    m_VertexArray->StoreFloatArrayBuffer(normals, size, Buffers::NORMAL_ARRAY_BUFFER, 3, Graphics::BufferUsageHint::StaticDraw);
}

void Pine::Mesh::SetTangents(float* tangents, std::size_t size)
{
    m_VertexArray->Bind();
    m_VertexArray->StoreFloatArrayBuffer(tangents, size, Buffers::TANGENT_ARRAY_BUFFER, 3, Graphics::BufferUsageHint::StaticDraw);
    m_HasTangentData = true;
}

void Pine::Mesh::SetUvs(float* uvs, std::size_t size)
{
    m_VertexArray->Bind();
    m_VertexArray->StoreFloatArrayBuffer(uvs, size, Buffers::UV_ARRAY_BUFFER, 2, Graphics::BufferUsageHint::StaticDraw);
}
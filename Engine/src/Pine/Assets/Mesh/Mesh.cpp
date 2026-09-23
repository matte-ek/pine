#include "Mesh.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"

using namespace Pine::Renderer3D::Specifications;

Pine::Mesh::Mesh(Model* model)
{
    m_Model = model;
    m_VertexArray = Graphics::GetGraphicsAPI()->CreateVertexArray();
}

void Pine::Mesh::Dispose()
{
    m_VertexArray->Dispose();

    Graphics::GetGraphicsAPI()->DestroyVertexArray(m_VertexArray);

    m_VertexArray = nullptr;

    // The vertex array owns the buffers it handed out, so disposing it leaves these dangling.
    m_VertexBuffer = nullptr;
    m_NormalBuffer = nullptr;
    m_TangentBuffer = nullptr;
    m_UvBuffer = nullptr;
}

Pine::Graphics::IVertexArray *Pine::Mesh::GetVertexArray() const
{
    return m_VertexArray;
}

std::uint32_t Pine::Mesh::GetRenderCount() const
{
    return m_RenderCount;
}

std::uint32_t Pine::Mesh::GetVertexCount() const
{
    return m_VertexCount;
}

bool Pine::Mesh::HasElementBuffer() const
{
    return m_HasElementBuffer;
}

bool Pine::Mesh::ReadGeometry(std::vector<Vector3f>& vertices, std::vector<std::uint32_t>& indices) const
{
    vertices.clear();
    indices.clear();

    if (m_VertexArray == nullptr || m_VertexBuffer == nullptr)
    {
        return false;
    }

    vertices.resize(m_VertexCount);

    if (!m_VertexBuffer->ReadData(vertices.data(), vertices.size() * sizeof(Vector3f)))
    {
        vertices.clear();
        return false;
    }

    if (!m_HasElementBuffer)
    {
        return true;
    }

    indices.resize(m_RenderCount);

    if (!m_VertexArray->ReadElementArrayBuffer(indices.data(), indices.size() * sizeof(std::uint32_t)))
    {
        vertices.clear();
        indices.clear();
        return false;
    }

    return true;
}

void Pine::Mesh::SetMaterial(Material*material)
{
    m_Material = material;

    if (material && !material->IsMeshGenerated() && m_Model)
    {
    }
}

void Pine::Mesh::SetMaterial(const UId id)
{
    m_Material = id;
}

Pine::Material *Pine::Mesh::GetMaterial() const
{
    return m_Material.Get();
}

const Pine::UId& Pine::Mesh::GetMaterialUId() const
{
    return m_Material.GetUId();
}

Pine::Model *Pine::Mesh::GetModel() const
{
    return m_Model;
}

const Pine::Vector3f& Pine::Mesh::GetBoundingBoxMin() const
{
    return m_BoundingBoxMin;
}

const Pine::Vector3f& Pine::Mesh::GetBoundingBoxMax() const
{
    return m_BoundingBoxMax;
}

void Pine::Mesh::SetVertices(float* vertices, const std::size_t size, const Graphics::BufferUsageHint usage)
{
    m_VertexArray->Bind();
    m_VertexBuffer = m_VertexArray->StoreFloatArrayBuffer(vertices, size, Buffers::VERTEX_ARRAY_BUFFER, 3, usage);
    m_VertexCount = static_cast<std::uint32_t>(size / sizeof(Vector3f));
    m_RenderCount = m_VertexCount;
}

void Pine::Mesh::SetIndices(std::uint32_t *indices, const std::size_t size)
{
    m_VertexArray->Bind();
    m_VertexArray->StoreElementArrayBuffer(indices, size);
    m_HasElementBuffer = true;
    m_RenderCount = static_cast<std::uint32_t>(size / sizeof(std::uint32_t));
}

void Pine::Mesh::SetNormals(float* normals, const std::size_t size, const Graphics::BufferUsageHint usage)
{
    m_VertexArray->Bind();
    m_NormalBuffer = m_VertexArray->StoreFloatArrayBuffer(normals, size, Buffers::NORMAL_ARRAY_BUFFER, 3, usage);
}

void Pine::Mesh::SetTangents(float* tangents, const std::size_t size, const Graphics::BufferUsageHint usage)
{
    m_VertexArray->Bind();
    m_TangentBuffer = m_VertexArray->StoreFloatArrayBuffer(tangents, size, Buffers::TANGENT_ARRAY_BUFFER, 3, usage);
    m_HasTangentData = true;
}

void Pine::Mesh::SetUvs(float* uvs, const std::size_t size, const Graphics::BufferUsageHint usage)
{
    m_VertexArray->Bind();
    m_UvBuffer = m_VertexArray->StoreFloatArrayBuffer(uvs, size, Buffers::UV_ARRAY_BUFFER, 2, usage);
}

// Uploads into one attribute buffer, refusing anything the buffer cannot hold. The buffer has to be
// bound before glBufferSubData reaches it, which is what Bind() is for here - the vertex array
// binding alone does not select it.
void Pine::Mesh::UploadAttribute(Graphics::IVertexBuffer* buffer,
                                 const char* name,
                                 const void* data,
                                 const std::size_t size,
                                 const std::size_t offset)
{
    if (buffer == nullptr)
    {
        PWarning(fmt::format("Ignored a mesh {} update: the attribute has no buffer yet.", name));
        return;
    }

    if (offset + size > buffer->GetSize())
    {
        PWarning(fmt::format("Ignored a mesh {} update of {} byte(s) at {}: its buffer holds {}.",
                             name, size, offset, buffer->GetSize()));
        return;
    }

    buffer->Bind();
    buffer->UploadData(data, size, offset);
}

void Pine::Mesh::UpdateVertices(const float* vertices, const std::size_t size, const std::size_t offset)
{
    UploadAttribute(m_VertexBuffer, "vertex", vertices, size, offset);
}

void Pine::Mesh::UpdateNormals(const float* normals, const std::size_t size, const std::size_t offset)
{
    UploadAttribute(m_NormalBuffer, "normal", normals, size, offset);
}

void Pine::Mesh::UpdateTangents(const float* tangents, const std::size_t size, const std::size_t offset)
{
    UploadAttribute(m_TangentBuffer, "tangent", tangents, size, offset);
}

void Pine::Mesh::UpdateUvs(const float* uvs, const std::size_t size, const std::size_t offset)
{
    UploadAttribute(m_UvBuffer, "uv", uvs, size, offset);
}

void Pine::Mesh::SetAABB(const Vector3f min, const Vector3f max)
{
    m_BoundingBoxMin = min;
    m_BoundingBoxMax = max;
}

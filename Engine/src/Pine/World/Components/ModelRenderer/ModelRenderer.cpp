#include "ModelRenderer.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"

Pine::ModelRenderer::ModelRenderer()
        : Component(ComponentType::ModelRenderer)
{
}

void Pine::ModelRenderer::SetModel(Model*model)
{
    m_Model = model;
}

Pine::Model *Pine::ModelRenderer::GetModel() const
{
    return m_Model.Get();
}

void Pine::ModelRenderer::SetOverrideMaterial(Material *material)
{
    m_OverrideMaterial = material;
}

Pine::Material * Pine::ModelRenderer::GetOverrideMaterial() const
{
    return m_OverrideMaterial.Get();
}

void Pine::ModelRenderer::SetOverrideStencilBuffer(bool value)
{
    m_OverrideStencilBuffer = value;
}

bool Pine::ModelRenderer::GetOverrideStencilBuffer() const
{
    return m_OverrideStencilBuffer;
}

void Pine::ModelRenderer::SetStencilBufferValue(int value)
{
    m_StencilBufferValue = value;
}

int Pine::ModelRenderer::GetStencilBufferValue() const
{
    return m_StencilBufferValue;
}

void Pine::ModelRenderer::SetModelMeshIndex(int index)
{
    m_ModelMeshIndex = index;
}

int Pine::ModelRenderer::GetModelMeshIndex() const
{
    return m_ModelMeshIndex;
}

Pine::Renderer3D::ModelRendererHintData& Pine::ModelRenderer::GetRenderingHintData()
{
    return m_RenderingHintData;
}

void Pine::ModelRenderer::LoadData(const ByteSpan& span)
{
    ModelRendererSerializer serializer;

    serializer.Read(span);

    serializer.Model.Read(m_Model);
    serializer.OverrideMaterial.Read(m_OverrideMaterial);
    serializer.MeshIndex.Read(m_ModelMeshIndex);
}

Pine::ByteSpan Pine::ModelRenderer::SaveData()
{
    ModelRendererSerializer serializer;

    serializer.Model.Write(m_Model);
    serializer.OverrideMaterial.Write(m_OverrideMaterial);
    serializer.MeshIndex.Write(m_ModelMeshIndex);

    return serializer.Write();
}

#include "ModelRenderer.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::ModelRenderer::ModelRenderer()
        : IComponent(ComponentType::ModelRenderer)
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

void Pine::ModelRenderer::LoadData(const nlohmann::json &j)
{
    Serialization::LoadAsset<Pine::Model>(j, "model", m_Model);
    Serialization::LoadAsset<Pine::Material>(j, "overrideMaterial", m_OverrideMaterial);
    Serialization::LoadValue(j, "modelMeshIndex", m_ModelMeshIndex);
}

void Pine::ModelRenderer::SaveData(nlohmann::json &j)
{
    j["model"] = Serialization::StoreAsset(m_Model);
    j["overrideMaterial"] = Serialization::StoreAsset(m_OverrideMaterial);
    j["modelMeshIndex"] = m_ModelMeshIndex;
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

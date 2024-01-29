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

void Pine::ModelRenderer::LoadData(const nlohmann::json &j)
{
    Serialization::LoadAsset<Pine::Model>(j, "model", m_Model);
}

void Pine::ModelRenderer::SaveData(nlohmann::json &j)
{
    j["model"] = Serialization::StoreAsset(m_Model);
}

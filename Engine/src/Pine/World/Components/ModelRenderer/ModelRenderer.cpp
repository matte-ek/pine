#include "ModelRenderer.hpp"

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

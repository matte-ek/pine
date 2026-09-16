#include "ModelRenderer.hpp"

#include "../../Values/Values.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;

    json Read(const Pine::Component* component)
    {
        const Pine::ModelRenderer defaults;
        const auto renderer = component == nullptr ? &defaults : static_cast<const Pine::ModelRenderer*>(component);
        return {
            { "Model", Values::AssetReference(renderer->GetModel()) },
            { "OverrideMaterial", Values::AssetReference(renderer->GetOverrideMaterial()) },
            { "MeshIndex", renderer->GetModelMeshIndex() }
        };
    }

    void Validate(const json& state, const std::string& path)
    {
        const auto index = state.at("MeshIndex").get<int>();
        const auto model = static_cast<Pine::Model*>(Values::ResolvedAsset(state.at("Model")));
        Values::Require(index == -1 || (model != nullptr && static_cast<std::size_t>(index) < model->GetMeshes().size()),
            path + "/MeshIndex", "Expected -1 (all meshes) or an index within the selected model.");
    }

    void Apply(Pine::Component* component, const json& state)
    {
        const auto renderer = static_cast<Pine::ModelRenderer*>(component);
        renderer->SetModel(static_cast<Pine::Model*>(Values::ResolvedAsset(state.at("Model"))));
        renderer->SetOverrideMaterial(static_cast<Pine::Material*>(Values::ResolvedAsset(state.at("OverrideMaterial"))));
        renderer->SetModelMeshIndex(state.at("MeshIndex").get<int>());
        renderer->GetParent()->SetDirty(true);
    }
}

const Editor::DebugServer::Editing::Components::Adapter&
Editor::DebugServer::Editing::Components::ModelRenderer::GetAdapter()
{
    static const Adapter adapter = {
        Pine::ComponentType::ModelRenderer, "ModelRenderer",
        {
            { "Model", { { "type", "asset" }, { "assetType", "Model" }, { "nullable", true } } },
            { "OverrideMaterial", { { "type", "asset" }, { "assetType", "Material" }, { "nullable", true } } },
            { "MeshIndex", { { "type", "integer" }, { "minimum", -1 }, { "maximum", 2147483647 },
                { "constraint", "-1 for all meshes, otherwise an index within Model" } } }
        },
        Read, Validate, Apply, true
    };
    return adapter;
}

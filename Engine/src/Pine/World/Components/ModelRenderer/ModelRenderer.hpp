#pragma once
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

namespace Pine
{
    class Light;

    namespace Renderer3D
    {
        struct ModelRendererHintData
        {
            bool HasComputedData = false;

            std::array<ComponentHandle<Light>, 4> LightSlotIndex = {};
        };
    }

    class ModelRenderer final : public IComponent
    {
        AssetHandle<Model> m_Model;
        AssetHandle<Material> m_OverrideMaterial;

        bool m_OverrideStencilBuffer = false;
        int m_StencilBufferValue = 0xFF;

        int m_ModelMeshIndex = -1;

        Renderer3D::ModelRendererHintData m_RenderingHintData;
    public:
        ModelRenderer();

        void SetModel(Model* model);
        Model* GetModel() const;

        void SetOverrideMaterial(Material* material);
        Material* GetOverrideMaterial() const;

        void SetOverrideStencilBuffer(bool value);
        bool GetOverrideStencilBuffer() const;

        void SetStencilBufferValue(int value);
        int GetStencilBufferValue() const;

        void SetModelMeshIndex(int index);
        int GetModelMeshIndex() const;

        Renderer3D::ModelRendererHintData& GetRenderingHintData();

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}

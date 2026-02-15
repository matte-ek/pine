#pragma once
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{
    class Light;

    namespace Renderer3D
    {
        struct ModelRendererHintData
        {
            bool HasPassedFrustumCulling = false;
            bool HasComputedData = false;
            std::array<ComponentHandle<Light>, 6> LightSlotIndex = {};
        };
    }

    class ModelRenderer final : public Component
    {
        AssetHandle<Model> m_Model;
        AssetHandle<Material> m_OverrideMaterial;

        bool m_OverrideStencilBuffer = false;
        int m_StencilBufferValue = 0xFF;

        int m_ModelMeshIndex = -1;

        Renderer3D::ModelRendererHintData m_RenderingHintData;

        struct ModelRendererSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(Model);
            PINE_SERIALIZE_ASSET(OverrideMaterial);
            PINE_SERIALIZE_PRIMITIVE(MeshIndex, Pine::Serialization::DataType::Int32);
        };
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

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}

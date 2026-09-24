#pragma once
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Rendering/Renderer3D/LightSlotData.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
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
            // Which lights reach this object. Held as the shared type rather than as loose fields
            // because terrain chunks are lit by the same rule and the same code assigns both.
            LightSlotData Lights;

            // World-space bounds, kept up to date by the scene processor.
            //
            // Cached on the object because bounds belong to the object alone - unlike visibility,
            // which belongs to (object, frustum) and therefore lives in a VisibilitySet. With
            // several frustums culling per frame, computing this once instead of per frustum is the
            // difference that matters.
            Vector3f BoundsMin = Vector3f(0.f);
            Vector3f BoundsMax = Vector3f(0.f);

            // What the bounds were built from: the transform's world version and the model's own
            // bounds. The model's bounds rather than the model, because a re-import rebuilds them on
            // the same object. While neither changes, the bounds are kept rather than rebuilt.
            std::uint64_t BoundsTransformVersion = 0;
            Vector3f BoundsModelMin = Vector3f(0.f);
            Vector3f BoundsModelMax = Vector3f(0.f);

            // Last frame's bounds, so "did this object move" is answered by comparing boxes. A
            // transform can be written without moving, and only a box that actually changed should
            // re-render the shadow views containing it, which is precisely the cost caching exists
            // to avoid.
            //
            // A mover invalidates the view it *left* as well as the one it entered, so both the old
            // and the new box have to be testable.
            Vector3f PreviousBoundsMin = Vector3f(0.f);
            Vector3f PreviousBoundsMax = Vector3f(0.f);

            // The model drawn this frame once its LOD is chosen, or null while the object is past
            // its model's cull distance. Chosen once per frame by the scene processor, which also
            // compares it against last frame's: a change of level moves no bounds, but it still
            // changes what the object casts into a cached shadow view.
            Model* LodModel = nullptr;
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

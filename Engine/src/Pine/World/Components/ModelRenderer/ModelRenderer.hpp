#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Assets/Model/Model.hpp"

namespace Pine
{

    class ModelRenderer final : public IComponent
    {
    private:
        AssetHandle<Model> m_Model;

        bool m_OverrideStencilBuffer = false;
        int m_StencilBufferValue = 0xFF;
    public:
        ModelRenderer();

        void SetModel(Model* model);
        Model* GetModel() const;

        void SetOverrideStencilBuffer(bool value);
        bool GetOverrideStencilBuffer() const;

        void SetStencilBufferValue(int value);
        int GetStencilBufferValue() const;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}

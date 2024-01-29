#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Assets/Model/Model.hpp"

namespace Pine
{

    class ModelRenderer : public IComponent
    {
    private:
        AssetHandle<Model> m_Model;
    public:
        ModelRenderer();

        void SetModel(Model* model);
        Model* GetModel() const;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}

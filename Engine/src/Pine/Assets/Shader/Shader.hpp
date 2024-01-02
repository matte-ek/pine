#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"
#include <optional>

namespace Pine
{

    class Shader : public IAsset
    {
    private:
        Graphics::IShaderProgram* m_ShaderProgram = nullptr;

        bool m_BaseShader = true;
        bool m_Ready = false;

        Pine::Shader* m_ParentShader = nullptr;

        Pine::Shader* m_DiscardShader = nullptr;

        std::vector<IAsset*> m_ShaderFiles;

        bool LoadShaderPackage(const nlohmann::json& j);
    public:
        Shader();

        Graphics::IShaderProgram* GetProgram() const;

        void MarkAsUpdated() override;
        bool HasBeenUpdated() const override;

        void SetReady(bool ready);
        bool IsReady() const;

        bool IsBaseShader() const;

        Pine::Shader* GetParentShader() const;

        Pine::Shader* GetDiscardShader() const;

        std::optional<std::string> GetShaderSourceFile(Graphics::ShaderType type) const;

        bool LoadFromJson(const nlohmann::json& j);

        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };

}
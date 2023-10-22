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

        std::vector<IAsset*> m_ShaderFiles;
    public:
        Shader();

        Graphics::IShaderProgram* GetProgram() const;

        void MarkAsUpdated() override;
        bool HasBeenUpdated() const override;

        void SetReady(bool ready);
        bool IsReady() const;

        bool IsBaseShader() const;

        Pine::Shader* GetParentShader() const;

        std::optional<std::string> GetShaderSourceFile(Graphics::ShaderType type) const;

        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };

}
#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"

namespace Pine
{

    class Shader : public IAsset
    {
    private:
        Graphics::IShaderProgram* m_ShaderProgram = nullptr;

        bool m_Ready = false;

        std::vector<Pine::IAsset*> m_ShaderFiles;
    public:
        Shader();

        Graphics::IShaderProgram* GetProgram() const;

        void MarkAsUpdated() override;
        bool HasBeenUpdated() const override;

        void SetReady(bool ready);
        bool IsReady() const;

        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
        
    };

}
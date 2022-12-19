#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"

namespace Pine
{

    class Shader : public IAsset
    {
    private:
        Graphics::IShaderProgram* m_ShaderProgram = nullptr;
    public:
        Shader();

        Graphics::IShaderProgram* GetProgram() const;

        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
        
    };

}
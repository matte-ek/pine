#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"
#include <optional>
#include <unordered_map>

namespace Pine
{

    enum class ShaderVersion
    {
        Default = 0,
        Discard = (1 << 0),
        PerformanceFast = (1 << 1)
    };

    class Shader : public Asset
    {
    private:
        std::vector<Graphics::IShaderProgram*> m_ShaderPrograms;
        std::vector<bool> m_ShaderProgramsReady;

        std::unordered_map<std::uint32_t, std::uint32_t> m_ShaderVersionsMap;

        bool m_BaseShader = true;

        Shader* m_ParentShader = nullptr;

        std::vector<Asset*> m_ShaderFiles;

        bool LoadShaderPackage(const nlohmann::json& j, std::uint32_t shaderVersion, const std::vector<std::string>& versionMacros);
    public:
        Shader();

        Graphics::IShaderProgram* GetProgram(ShaderVersion version = ShaderVersion::Default) const;

        bool HasShaderVersion(ShaderVersion version) const;

        void MarkAsUpdated() override;
        bool HasBeenUpdated() const override;

        void SetReady(bool ready, ShaderVersion version = ShaderVersion::Default);
        bool IsReady(ShaderVersion version = ShaderVersion::Default);

        bool IsBaseShader() const;

        Shader* GetParentShader() const;

        std::optional<std::string> GetShaderSourceFile(Graphics::ShaderType type) const;

        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };

}
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

    struct ShaderTextureSamplerEntry
    {
        char VariableName[64];
        int Binding;
    };

    struct ShaderVersionEntry
    {
        char Name[64];
        std::uint32_t Index;
    };

    class Shader : public Asset
    {
    private:
        std::vector<Graphics::IShaderProgram*> m_ShaderPrograms;
        std::vector<bool> m_ShaderProgramsReady;
        std::vector<ShaderTextureSamplerEntry> m_ShaderTextureSamplerBindings;

        std::vector<ShaderVersionEntry> m_ShaderVersions;
        std::unordered_map<std::uint32_t, std::uint32_t> m_ShaderVersionsMap;

        bool LoadShaderPackage(const nlohmann::json& j, std::uint32_t shaderVersion, const std::vector<std::string>& versionMacros);
        bool LoadAssetData(const ByteSpan& span) override;

        struct ShaderSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_STRING(VertexSource);
            PINE_SERIALIZE_STRING(FragmentSource);
            PINE_SERIALIZE_STRING(ComputeSource);
            PINE_SERIALIZE_STRING(GeometrySource);
            PINE_SERIALIZE_ARRAY_FIXED(TextureSamplers, ShaderTextureSamplerEntry);
            PINE_SERIALIZE_ARRAY_FIXED(Versions, ShaderVersionEntry);
        };
    public:
        Shader();

        Graphics::IShaderProgram* GetProgram(ShaderVersion version = ShaderVersion::Default) const;

        bool HasShaderVersion(ShaderVersion version) const;

        void SetReady(bool ready, ShaderVersion version = ShaderVersion::Default);
        bool IsReady(ShaderVersion version = ShaderVersion::Default);

        std::optional<std::string> GetShaderSourceFile(Graphics::ShaderType type) const;

        void Dispose() override;
    };

}
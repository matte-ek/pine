#pragma once

#include <unordered_map>
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"

namespace Pine
{
    namespace Importer
    {
        class ShaderImporter;
    }

    using ShaderVersion = uint32_t;

    struct ShaderTextureSamplerEntry
    {
        char VariableName[64];
        int Binding;
    };

    struct ShaderVersionEntry
    {
        char Name[64];
        std::uint32_t Bit;
    };

    class Shader : public Asset
    {
    private:
        std::vector<Graphics::IShaderProgram*> m_ShaderPrograms;
        std::vector<ShaderTextureSamplerEntry> m_ShaderTextureSamplerBindings;

        std::vector<bool> m_ShaderRendererReady;

        std::vector<ShaderVersionEntry> m_ShaderVersions;
        std::unordered_map<std::uint32_t, std::uint32_t> m_ShaderVersionsMap;

        std::array<std::string, static_cast<size_t>(Graphics::ShaderType::ShaderTypeCount)> m_ShaderSources;

        bool CompileShader(Graphics::IShaderProgram* program, Graphics::ShaderType shaderType, const std::vector<std::string>& versionMacros) const;

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

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

        Graphics::IShaderProgram* GetProgram(ShaderVersion version = 0) const;

        bool HasShaderVersion(ShaderVersion version) const;
        bool CompileShaderVersion(ShaderVersion version);

        void SetRendererReady(bool ready, ShaderVersion version = 0);
        bool IsRendererReady(ShaderVersion version = 0);

        void AddVersion(const std::string& name, std::uint32_t bit);
        void AddTextureSamplerBinding(const std::string& name, int binding);

        bool Import() override;

        void Dispose() override;

        friend class Importer::ShaderImporter;
    };

}
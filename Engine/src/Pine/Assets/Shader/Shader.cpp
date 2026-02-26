#include "Shader.hpp"

#include <string>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "Importer/ShaderImporter.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"
#include "Pine/Threading/Threading.hpp"

namespace
{
    using namespace Pine;

    constexpr std::array<const char*, 4> ShaderTypesString = { "Vertex", "Fragment", "Compute", "Geometry" };
}

Shader::Shader()
{
    m_Type = AssetType::Shader;
}

bool Shader::CompileShader(
    Graphics::IShaderProgram* program,
    Graphics::ShaderType shaderType,
    const std::vector<std::string>& versionMacros) const
{
    auto shaderSource = m_ShaderSources[static_cast<std::uint32_t>(shaderType)];

    // Insert any macros for pre-processor if we have to
    if (!versionMacros.empty())
    {
        // Since we need to add the macros after the version, find that first
        const auto offset = shaderSource.find('\n') + 1;

        for (const auto& ver : versionMacros)
        {
            shaderSource = shaderSource.insert(offset, fmt::format("#define {}\n", ver));
        }
    }

    if (!program->CompileAndLoadShader(shaderSource, shaderType))
    {
        Log::Error(fmt::format("Error occurred in type {}", ShaderTypesString[static_cast<std::uint32_t>(shaderType)]));
        return false;
    }

    return true;
}

bool Shader::LoadAssetData(const ByteSpan& span)
{
    ShaderSerializer shaderSerializer;

    if (!shaderSerializer.Read(span))
    {
        return false;
    }

    // Read general shader data
    shaderSerializer.TextureSamplers.Read(m_ShaderTextureSamplerBindings);
    shaderSerializer.Versions.Read(m_ShaderVersions);

    // Read shader source
    std::string vertexSource;
    std::string fragmentSource;
    std::string geometrySource;
    std::string computeSource;

    shaderSerializer.VertexSource.Read(vertexSource);
    shaderSerializer.FragmentSource.Read(fragmentSource);
    shaderSerializer.GeometrySource.Read(geometrySource);
    shaderSerializer.ComputeSource.Read(computeSource);

    // Early exit for non-existent data
    if (vertexSource.empty() && fragmentSource.empty() && geometrySource.empty() && computeSource.empty())
    {
        return false;
    }

    // Store shader source for future compilations
    m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Vertex)] = vertexSource;
    m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Fragment)] = fragmentSource;
    m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Geometry)] = geometrySource;
    m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Compute)] = computeSource;

    auto task = Threading::QueueTask<void>([this]()
    {
        // Make sure "main" version of the shader is compiled.
        CompileShaderVersion(0);
    },
    TaskThreadingMode::MainThread);

    // Wait for the main one to compile
    Threading::AwaitTaskResult(task);

    return true;
}

Graphics::IShaderProgram* Shader::GetProgram(ShaderVersion version) const
{
    // surely the user has called HasShaderVersion(...) beforehand and this won't ever crash
    return m_ShaderPrograms[m_ShaderVersionsMap.at(static_cast<std::uint32_t>(version))];
}

bool Shader::HasShaderVersion(ShaderVersion version) const
{
    if (version == 0)
    {
        return !m_ShaderPrograms.empty();
    }

    return m_ShaderVersionsMap.count(static_cast<std::uint32_t>(version)) != 0;
}

bool Shader::CompileShaderVersion(ShaderVersion version)
{
    if (HasShaderVersion(version))
    {
        const auto shaderProgramIndex = m_ShaderVersionsMap[version];

        auto shaderProgram = m_ShaderPrograms[shaderProgramIndex];
        Graphics::GetGraphicsAPI()->DestroyShaderProgram(shaderProgram);

        m_ShaderVersionsMap.erase(version);
        m_ShaderPrograms.erase(m_ShaderPrograms.begin() + shaderProgramIndex);
        m_ShaderRendererReady.erase(m_ShaderRendererReady.begin() + shaderProgramIndex);
    }

    // Figure out what version macros to use
    std::vector<std::string> versionMacros;
    for (const auto& [Name, Bit] : m_ShaderVersions)
    {
        if (version & Bit)
        {
            versionMacros.emplace_back(Name);
        }
    }

    // Prepare a new program
    auto shaderProgram = Graphics::GetGraphicsAPI()->CreateShaderProgram();

    // Compile all the shaders
    for (size_t i{};i < m_ShaderSources.size();i++)
    {
        if (m_ShaderSources[i].empty())
        {
            continue;
        }

        if (!CompileShader(shaderProgram, static_cast<Graphics::ShaderType>(i), versionMacros))
        {
            Graphics::GetGraphicsAPI()->DestroyShaderProgram(shaderProgram);
            return false;
        }
    }

    if (!shaderProgram->LinkProgram())
    {
        Graphics::GetGraphicsAPI()->DestroyShaderProgram(shaderProgram);
        return false;
    }

    shaderProgram->Use();

    for (const auto& sampler : m_ShaderTextureSamplerBindings)
    {
        auto uniform = shaderProgram->GetUniformVariable(sampler.VariableName);
        if (!uniform)
        {
            Log::Warning(fmt::format("Failed to find '{}' uniform buffer when attaching samplers for shader {}", sampler.VariableName, m_Path));
            continue;
        }

        uniform->LoadInteger(sampler.Binding);
    }

    m_ShaderPrograms.push_back(shaderProgram);
    m_ShaderRendererReady.push_back(false);
    m_ShaderVersionsMap[version] = m_ShaderPrograms.size() - 1;

    return true;
}

void Shader::SetRendererReady(bool ready, ShaderVersion version)
{
    if (!HasShaderVersion(version))
    {
        return;
    }

    m_ShaderRendererReady[m_ShaderVersionsMap[version]] = ready;
}

bool Shader::IsRendererReady(ShaderVersion version)
{
    if (!HasShaderVersion(version))
    {
        return m_ShaderRendererReady[0];
    }

    return m_ShaderRendererReady[m_ShaderVersionsMap[version]];
}

void Shader::AddVersion(const std::string& name, std::uint32_t bit)
{
    if (name.size() > 63)
    {
        throw std::logic_error("Name too large.");
    }

    ShaderVersionEntry entry {};

    std::snprintf(entry.Name, sizeof(entry.Name), "%s", name.c_str());

    entry.Bit = bit;

    m_ShaderVersions.push_back(entry);
}

void Shader::AddTextureSamplerBinding(const std::string& name, int binding)
{
    if (name.size() > 63)
    {
        throw std::logic_error("Name too large.");
    }

    ShaderTextureSamplerEntry entry {};

    std::snprintf(entry.VariableName, sizeof(entry.VariableName), "%s", name.c_str());

    entry.Binding = binding;

    m_ShaderTextureSamplerBindings.push_back(entry);
}

bool Shader::Import()
{
    return Importer::ShaderImporter::Import(this);
}

ByteSpan Shader::SaveAssetData()
{
    ShaderSerializer serializer;

    serializer.VertexSource.Write(m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Vertex)]);
    serializer.FragmentSource.Write(m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Fragment)]);
    serializer.GeometrySource.Write(m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Geometry)]);
    serializer.ComputeSource.Write(m_ShaderSources[static_cast<std::uint32_t>(Graphics::ShaderType::Compute)]);

    serializer.TextureSamplers.Write(m_ShaderTextureSamplerBindings);
    serializer.Versions.Write(m_ShaderVersions);

    return serializer.Write();
}

void Shader::Dispose()
{
    for (const auto shaderProgram : m_ShaderPrograms)
    {
        Graphics::GetGraphicsAPI()->DestroyShaderProgram(shaderProgram);
    }

    m_ShaderPrograms.clear();
    m_ShaderRendererReady.clear();

    m_State = AssetState::Unloaded;
}

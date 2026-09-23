#include "Shader.hpp"

#include <cstring>

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
#include "Pine/Rendering/Renderer3D/Specifications.hpp"

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

    // Everything injected here goes immediately after the #version directive, which every shader
    // source starts with on line 1.
    const auto offset = shaderSource.find('\n') + 1;

    // Array sizes and binding points shared between C++ and GLSL, injected rather than written twice.
    //
    // Hand-syncing them has already cost us once: MAX_INSTANCE_COUNT was 512 in Specifications.hpp
    // while the shaders declared instances[128], so instances 128-511 read out of bounds with
    // nothing reporting it. A UBO array size is exactly the kind of constant that gets bumped on
    // one side only, so the shaders now read it from the one place it is defined.
    const auto sharedDefines = fmt::format(
        "#define MAX_INSTANCE_COUNT {}\n"
        "#define DYNAMIC_LIGHT_COUNT {}\n"
        "#define SHADOW_VIEW_COUNT {}\n"
        "#define TERRAIN_DETAIL_INSTANCE_BINDING {}\n",
        Renderer3D::Specifications::General::MAX_INSTANCE_COUNT,
        Renderer3D::Specifications::General::DYNAMIC_LIGHT_COUNT,
        Renderer3D::Specifications::Shadows::SHADOW_VIEW_COUNT,
        Renderer3D::Specifications::StorageBuffers::TERRAIN_DETAIL_INSTANCES);

    shaderSource = shaderSource.insert(offset, sharedDefines);

    // Insert any macros for pre-processor if we have to
    if (!versionMacros.empty())
    {
        for (const auto& ver : versionMacros)
        {
            shaderSource = shaderSource.insert(offset, fmt::format("#define {}\n", ver));
        }
    }

    shaderSource.erase(std::remove_if(shaderSource.begin(), shaderSource.end(), [](const char c){return !(c>=0 && c <128);}), shaderSource.end());

    if (!program->CompileAndLoadShader(shaderSource, shaderType))
    {
        PError(fmt::format("Error occurred in type {} in file {}", ShaderTypesString[static_cast<std::uint32_t>(shaderType)], m_FilePath.string()));
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

    bool compiled = false;

    auto task = Threading::QueueTask<void>([this, &compiled]()
    {
        // Make sure "main" version of the shader is compiled.
        compiled = CompileShaderVersion(0);
    },
    TaskThreadingMode::MainThread);

    // Wait for the main one to compile
    Threading::AwaitTaskResult(task);

    // Without a program there is nothing usable here, and every GetProgram() caller would fault on
    // it. CompileShader() has already logged which stage failed.
    return compiled;
}

Graphics::IShaderProgram* Shader::GetProgram(const ShaderVersion version) const
{
    // surely the user has called HasShaderVersion(...) beforehand and this won't ever crash
    return m_ShaderPrograms[m_ShaderVersionsMap.at(static_cast<std::uint32_t>(version))];
}

bool Shader::HasShaderVersion(const ShaderVersion version) const
{
    if (version == 0)
    {
        return !m_ShaderPrograms.empty();
    }

    return m_ShaderVersionsMap.count(static_cast<std::uint32_t>(version)) != 0;
}

bool Shader::CompileShaderVersion(const ShaderVersion version)
{
    // Figure out what version macros to use
    std::vector<std::string> versionMacros;
    for (const auto& [Name, Bit] : m_ShaderVersions)
    {
        if (version & Bit)
        {
            versionMacros.emplace_back(Name);
        }
    }

    // Prepare a new program. A version that is already compiled stays untouched until this one has
    // linked, so re-compiling a shader that no longer builds (a typo saved while the editor is
    // open, for instance) keeps rendering with the last working program instead of leaving the
    // asset without one - which every GetProgram() caller would then fault on.
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
            PWarning(fmt::format("Failed to find '{}' uniform buffer when attaching samplers for shader {}", sampler.VariableName, m_Path));
            continue;
        }

        uniform->LoadInteger(sampler.Binding);
    }

    const auto existingVersion = m_ShaderVersionsMap.find(version);

    if (existingVersion != m_ShaderVersionsMap.end())
    {
        // Replace the program in its existing slot rather than erasing it: the map holds indices
        // into m_ShaderPrograms, so removing an element would leave every version compiled after
        // this one pointing at its neighbour.
        const auto shaderProgramIndex = existingVersion->second;

        Graphics::GetGraphicsAPI()->DestroyShaderProgram(m_ShaderPrograms[shaderProgramIndex]);

        m_ShaderPrograms[shaderProgramIndex] = shaderProgram;
        m_ShaderRendererReady[shaderProgramIndex] = false;

        return true;
    }

    m_ShaderPrograms.push_back(shaderProgram);
    m_ShaderRendererReady.push_back(false);
    m_ShaderVersionsMap[version] = m_ShaderPrograms.size() - 1;

    return true;
}

void Shader::SetRendererReady(const bool ready, const ShaderVersion version)
{
    if (!HasShaderVersion(version))
    {
        return;
    }

    m_ShaderRendererReady[m_ShaderVersionsMap[version]] = ready;
}

bool Shader::IsRendererReady(const ShaderVersion version)
{
    if (!HasShaderVersion(version))
    {
        return m_ShaderRendererReady[0];
    }

    return m_ShaderRendererReady[m_ShaderVersionsMap[version]];
}

void Shader::AddVersion(const std::string& name, const std::uint32_t bit)
{
    if (name.size() > 63)
    {
        throw std::logic_error("Name too large.");
    }

    ShaderVersionEntry entry {};

    std::snprintf(entry.Name, sizeof(entry.Name), "%s", name.c_str());

    entry.Bit = bit;

    // Replaced rather than appended, because this is called again every time the shader is
    // re-imported - which a source file saved while the editor is open does. Appending would put
    // the same name in the list twice, and CompileShaderVersion turns each entry into a #define, so
    // the second copy would make the shader fail to compile on a macro redefinition. Re-import does
    // not clear the list first, deliberately: versions may have come from an .ih file that a
    // source-only re-import never reads.
    for (auto& existing : m_ShaderVersions)
    {
        if (std::strcmp(existing.Name, entry.Name) == 0)
        {
            existing = entry;

            return;
        }
    }

    m_ShaderVersions.push_back(entry);
}

void Shader::AddTextureSamplerBinding(const std::string& name, const int binding)
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

bool Shader::Import(Importer::AssetImport* context)
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

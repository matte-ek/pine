#include "Shader.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"

#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <string>

namespace
{
    const std::array<const char*, 3> ShaderTypesString = { "Vertex", "Fragment", "Compute" };

    bool LoadAndCompileShader(const std::string& filePath,
                              const nlohmann::json& json,
                              Pine::Graphics::IShaderProgram* program,
                              Pine::Shader* shader,
                              Pine::Graphics::ShaderType type,
                              const std::vector<std::string>& versionMacros)
    {
        assert(ShaderTypesString.size() == static_cast<int>(Pine::Graphics::ShaderType::ShaderTypeCount));

        const auto shaderTypeStr = ShaderTypesString[static_cast<int>(type)];
        const auto hasParentShader = !shader->IsBaseShader();
        const auto parentShader = shader->GetParentShader();

        if (!std::filesystem::exists(filePath) && !hasParentShader)
        {
            Pine::Log::Error("Failed to find file " + filePath);
            return false;
        }

        std::string file = Pine::File::ReadFile(filePath).value_or("");
        std::string src;

        if (file.empty() && !hasParentShader)
        {
            Pine::Log::Error("Failed to read file " + filePath);
            return false;
        }

        if (hasParentShader)
        {
            const auto parentShaderSource = Pine::File::ReadFile(parentShader->GetShaderSourceFile(type).value()).value();

            src = Pine::String::Replace(parentShaderSource, "#shader hooks", file);

            if (json.contains("hooks"))
            {
                for (const auto& hook : json["hooks"].items())
                {
                    if (!Pine::String::EndsWith(hook.key(), shaderTypeStr))
                        continue;

                    if (Pine::String::StartsWith(hook.key(), "Pre"))
                        src = Pine::String::Replace(src, fmt::format("#shader pre{}", shaderTypeStr), fmt::format("{}();", hook.value().get<std::string>()));
                    if (Pine::String::StartsWith(hook.key(), "Post"))
                        src = Pine::String::Replace(src, fmt::format("#shader post{}", shaderTypeStr), fmt::format("{}();", hook.value().get<std::string>()));
                }
            }

            Pine::Log::Info(src);
        }
        else
        {
            src = file;
        }

        // Remove remaining stuff from the source.
        src = Pine::String::Replace(src, fmt::format("#shader pre{}", shaderTypeStr), "");
        src = Pine::String::Replace(src, fmt::format("#shader post{}", shaderTypeStr), "");
        src = Pine::String::Replace(src, "#shader hooks", "");

        // Insert any macros for pre-processor if we have to
        if (!versionMacros.empty())
        {
            // Since we need to add the macros after the version, find that first
            const auto offset = src.find('\n') + 1;

            for (const auto& macro : versionMacros)
            {
                src = src.insert(offset, fmt::format("#define {}\n", macro));
            }
        }

        if (!program->CompileAndLoadShader(src, type))
        {
            Pine::Log::Error(fmt::format("Error occurred in file {}", filePath));

            return false;
        }

        return true;
    }

    // Gets asset's parent directory and adds that to the path specified
    // in the JSON file. We do this because the file path specified in the JSON
    // is relative to the .shader file, and we need the path relative to the application
    std::string GetAbsolutePath(const Pine::Shader* shader, const std::string& filePath)
    {
        const auto assetParentDirectory = shader->GetFilePath().parent_path().string();

        return assetParentDirectory + "/" + filePath;
    }

}

Pine::Shader::Shader()
{
    m_Type = AssetType::Shader;
}

bool Pine::Shader::LoadFromFile(AssetLoadStage stage)
{
    // Attempt to parse JSON
    auto jsonOpt = Serialization::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
        return false;

    const auto j = jsonOpt.value();

    // First we'll always try to deal with the default version
    if (!LoadShaderPackage(j, 0, {}))
    {
        return false;
    }

    if (j.contains("versions"))
    {
        for (const auto& version : j["versions"].items())
        {
            // We'll most likely want to deal with combinations of different versions in the future, however
            // we want some pre-defined combinations as I don't want it to be compiling every unique version possible.
            if (!LoadShaderPackage(j, version.value().get<std::uint32_t>(), { version.key() }))
            {
                return false;
            }
        }
    }

    m_State = AssetState::Loaded;

    return true;
}

void Pine::Shader::Dispose()
{
    for (auto shaderProgram : m_ShaderPrograms)
    {
        Graphics::GetGraphicsAPI()->DestroyShaderProgram(shaderProgram);
    }

    m_ShaderPrograms.clear();
    m_ShaderFiles.clear();

    m_Ready = false;

    m_State = AssetState::Unloaded;
}

Pine::Graphics::IShaderProgram* Pine::Shader::GetProgram(ShaderVersion version) const
{
    // surely the user has called HasShaderVersion(...) beforehand and this won't ever crash
    return m_ShaderPrograms[m_ShaderVersionsMap.at(static_cast<std::uint32_t>(version))];
}

bool Pine::Shader::HasBeenUpdated() const
{
    if (IAsset::HasBeenUpdated())
    {
        return true;
    }

    for (const auto& shaderFile : m_ShaderFiles)
    {
        if (shaderFile->HasBeenUpdated())
        {
            return true;
        }
    }

    return false;
}

void Pine::Shader::MarkAsUpdated()
{
    IAsset::MarkAsUpdated();

    for (const auto& shaderFile : m_ShaderFiles)
    {
        shaderFile->MarkAsUpdated();
    }
}

void Pine::Shader::SetReady(bool ready)
{
    m_Ready = ready;
}

bool Pine::Shader::IsReady() const
{
    return m_Ready;
}

bool Pine::Shader::IsBaseShader() const
{
    return m_BaseShader;
}

Pine::Shader* Pine::Shader::GetParentShader() const
{
    return m_ParentShader;
}

std::optional<std::string> Pine::Shader::GetShaderSourceFile(Graphics::ShaderType type) const
{
    // Attempt to parse JSON
    auto jsonOpt = Serialization::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
        return std::nullopt;

    const auto j = jsonOpt.value();
    const auto shaderTypeString = Pine::String::ToLower(ShaderTypesString[static_cast<int>(type)]);

    if (!j.contains(shaderTypeString))
        return std::nullopt;

    return std::make_optional(GetAbsolutePath(this, j[shaderTypeString]));
}

bool Pine::Shader::LoadShaderPackage(const nlohmann::json &j, std::uint32_t shaderVersion, const std::vector<std::string>& versionMacros)
{
    // Get all shader file paths
    std::string vertexPath, fragmentPath, computePath;

    if (j.contains("vertex"))
        vertexPath = j["vertex"];
    if (j.contains("fragment"))
        fragmentPath = j["fragment"];
    if (j.contains("compute"))
        vertexPath = j["compute"];

    // Invalid shader configuration
    if (vertexPath.empty() && fragmentPath.empty() && computePath.empty())
    {
        Log::Error("No shader files specified in " + m_FileName);

        return false;
    }

    // Prepare graphics shader program
    auto shaderProgram = Graphics::GetGraphicsAPI()->CreateShaderProgram();

    if (j.contains("parent"))
    {
        m_ParentShader = Pine::Assets::Get<Pine::Shader>(j["parent"]);
        m_BaseShader = false;

        if (!m_ParentShader)
        {
            Pine::Log::Error(fmt::format("Failed to find parent shader in {}, parent shader: {}.", m_FileName, j["parent"].get<std::string>()));
            return false;
        }
    }

    // Load and compile all specified shaders
    if (!vertexPath.empty())
    {
        if (!LoadAndCompileShader(GetAbsolutePath(this, vertexPath), j, shaderProgram, this, Graphics::ShaderType::Vertex, versionMacros))
        {
            return false;
        }

        if (versionMacros.empty())
        {
            m_ShaderFiles.push_back(Assets::GetOrLoad(GetAbsolutePath(this, vertexPath)));
        }
    }

    if (!fragmentPath.empty())
    {
        if (!LoadAndCompileShader(GetAbsolutePath(this, fragmentPath), j, shaderProgram, this, Graphics::ShaderType::Fragment, versionMacros))
        {
            return false;
        }

        if (versionMacros.empty())
        {
            m_ShaderFiles.push_back(Assets::GetOrLoad(GetAbsolutePath(this, fragmentPath)));
        }
    }

    if (!computePath.empty())
    {
        if (!LoadAndCompileShader(GetAbsolutePath(this, computePath), j, shaderProgram, this, Graphics::ShaderType::Compute, versionMacros))
        {
            return false;
        }

        if (versionMacros.empty())
        {
            m_ShaderFiles.push_back(Assets::GetOrLoad(GetAbsolutePath(this, computePath)));
        }
    }

    // Finally attempt to link the program
    if (!shaderProgram->LinkProgram())
    {
        return false;
    }

    shaderProgram->Use();

    // Setup texture samplers
    if (j.contains("texture_samplers"))
    {
        std::vector<std::pair<std::string, int>> textureSamplers;

        for (const auto& item : j["texture_samplers"].items())
        {
            textureSamplers.emplace_back(item.key(), item.value().get<int>());
        }

        if (m_ParentShader != nullptr)
        {
            auto parentJson = Serialization::LoadFromFile(m_ParentShader->GetFilePath()).value();

            for (const auto& item : parentJson["texture_samplers"].items())
            {
                textureSamplers.emplace_back(item.key(), item.value().get<int>());
            }
        }

        for (const auto& [name, value] : textureSamplers)
        {
            auto uniformVariable = shaderProgram->GetUniformVariable(name);

            if (uniformVariable == nullptr)
            {
                Log::Warning("Failed to find texture sampler " + name + ", " + m_FileName);
                continue;
            }

            uniformVariable->LoadInteger(value);
        }
    }

    m_ShaderPrograms.push_back(shaderProgram);
    m_ShaderVersionsMap[shaderVersion] = m_ShaderPrograms.size() - 1;

    return true;
}

bool Pine::Shader::HasShaderVersion(Pine::ShaderVersion version) const
{
    if (version == ShaderVersion::Default)
    {
        return !m_ShaderPrograms.empty();
    }

    return m_ShaderVersionsMap.count(static_cast<std::uint32_t>(version)) != 0;
}

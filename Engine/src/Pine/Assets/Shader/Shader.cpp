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

    struct ShaderHook
    {
        std::string Name; // ex "PreVertex"
        std::string HookMacro; // ex "#PreVertex"
        Pine::Graphics::ShaderType Type;
    }; 

    const std::array<const char*, 3> StrShaderTypes = { "Vertex", "Fragment", "Compute" };

    bool LoadAndCompileShader(const std::string& filePath,
                              const nlohmann::json& json,
                              Pine::Shader* shader,
                              Pine::Graphics::ShaderType type)
    {
        assert(StrShaderTypes.size() == static_cast<int>(Pine::Graphics::ShaderType::ShaderTypeCount));

        const auto shaderTypeStr = StrShaderTypes[static_cast<int>(type)];
        const auto hasParentShader = !shader->IsBaseShader();
        const auto parentShader = shader->GetParentShader();

        auto program = shader->GetProgram();

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

    bool LoadShaderPackage(nlohmann::json& j)
    {
        return false;
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

    if (!LoadShaderPackage(j))
    {
        return false;
    }

    if (j.contains("generate_discard"))
    {
        auto discardJson = j;

        discardJson[""];
    }

    m_State = AssetState::Loaded;

    return true;
}

void Pine::Shader::Dispose()
{
    if (m_ShaderProgram)
    {
        Graphics::GetGraphicsAPI()->DestroyShaderProgram(m_ShaderProgram);

        m_ShaderProgram = nullptr;
    }

    m_ShaderFiles.clear();
    m_Ready = false;

    m_State = AssetState::Unloaded;
}

Pine::Graphics::IShaderProgram* Pine::Shader::GetProgram() const
{
    return m_ShaderProgram;
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
    const auto shaderTypeString = Pine::String::ToLower(StrShaderTypes[static_cast<int>(type)]);

    if (!j.contains(shaderTypeString))
        return std::nullopt;
    
    return std::make_optional(GetAbsolutePath(this, j[shaderTypeString]));
}

bool Pine::Shader::LoadShaderPackage(const nlohmann::json &j)
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
    m_ShaderProgram = Graphics::GetGraphicsAPI()->CreateShaderProgram();

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
        if (!LoadAndCompileShader(GetAbsolutePath(this, vertexPath), j, this, Graphics::ShaderType::Vertex))
        {
            return false;
        }

        m_ShaderFiles.push_back(Assets::GetOrLoad(GetAbsolutePath(this, vertexPath)));
    }

    if (!fragmentPath.empty())
    {
        if (!LoadAndCompileShader(GetAbsolutePath(this, fragmentPath), j, this, Graphics::ShaderType::Fragment))
        {
            return false;
        }

        m_ShaderFiles.push_back(Assets::GetOrLoad(GetAbsolutePath(this, fragmentPath)));
    }

    if (!computePath.empty())
    {
        if (!LoadAndCompileShader(GetAbsolutePath(this, computePath), j, this, Graphics::ShaderType::Compute))
        {
            return false;
        }

        m_ShaderFiles.push_back(Assets::GetOrLoad(GetAbsolutePath(this, computePath)));
    }

    // Finally attempt to link the program
    if (!m_ShaderProgram->LinkProgram())
    {
        return false;
    }

    m_ShaderProgram->Use();

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
            auto uniformVariable = m_ShaderProgram->GetUniformVariable(name);

            if (uniformVariable == nullptr)
            {
                Log::Warning("Failed to find texture sampler " + name + ", " + m_FileName);
                continue;
            }

            uniformVariable->LoadInteger(value);
        }
    }

    return true;
}

bool Pine::Shader::LoadFromJson(const nlohmann::json &j)
{
    return LoadShaderPackage(j);
}

Pine::Shader *Pine::Shader::GetDiscardShader() const
{
    return m_DiscardShader;
}

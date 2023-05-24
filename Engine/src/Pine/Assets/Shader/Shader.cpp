#include "Shader.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Rendering.hpp"

#include <fstream>

namespace
{

    bool LoadAndCompileShader(const std::string& filePath,
                              Pine::Graphics::IShaderProgram* shader,
                              Pine::Graphics::ShaderType type)
    {
        if (!std::filesystem::exists(filePath))
        {
            Pine::Log::Error("Failed to find file " + filePath);
            return false;
        }

        std::ifstream stream(filePath);

        if (!stream.is_open())
        {
            return false;
        }

        std::string src((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        stream.close();

        if (!shader->CompileAndLoadShader(src, type))
        {
            return false;
        }

        return true;
    }

}

Pine::Shader::Shader()
{
    m_Type = AssetType::Shader;
}

bool Pine::Shader::LoadFromFile(Pine::AssetLoadStage stage)
{
    // Attempt to parse JSON
    auto jsonOpt = Serialization::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
        return false;

    const auto j = jsonOpt.value();

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

    // Gets asset's parent directory and adds that to the path specified
    // in the JSON file. We do this because the file path specified in the JSON
    // is relative to the .shader file, and we need the path relative to the application
    const auto getRelativePath = [&](const std::string& filePath) {
        const auto assetParentDirectory = m_FilePath.parent_path().string();

        return assetParentDirectory + "/" + filePath;
    };

    // Prepare graphics shader program
    m_ShaderProgram = Graphics::GetGraphicsAPI()->CreateShaderProgram();

    // Load and compile all specified shaders
    if (!vertexPath.empty())
    {
        if (!LoadAndCompileShader(getRelativePath(vertexPath), m_ShaderProgram, Graphics::ShaderType::Vertex))
        {
            return false;
        }
    }

    if (!fragmentPath.empty())
    {
        if (!LoadAndCompileShader(getRelativePath(fragmentPath), m_ShaderProgram, Graphics::ShaderType::Fragment))
        {
            return false;
        }
    }

    if (!computePath.empty())
    {
        if (!LoadAndCompileShader(getRelativePath(computePath), m_ShaderProgram, Graphics::ShaderType::Compute))
        {
            return false;
        }
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
        for (const auto& item : j["texture_samplers"].items())
        {
            auto uniformVariable = m_ShaderProgram->GetUniformVariable(item.key());

            if (uniformVariable == nullptr)
            {
                Log::Warning("Failed to find texture sampler " + item.key() + ", " + m_FileName);
                continue;
            }

            uniformVariable->LoadInteger(item.value().get<int>());
        }
    }

    // TODO: Setup uniform buffers

    m_State = AssetState::Loaded;

    return true;
}

void Pine::Shader::Dispose()
{
    if (m_ShaderProgram)
    {
        m_ShaderProgram->Dispose();

        Graphics::GetGraphicsAPI()->DestroyShaderProgram(m_ShaderProgram);

        m_ShaderProgram = nullptr;
    }

    m_State = AssetState::Unloaded;
}

Pine::Graphics::IShaderProgram* Pine::Shader::GetProgram() const
{
    return m_ShaderProgram;
}

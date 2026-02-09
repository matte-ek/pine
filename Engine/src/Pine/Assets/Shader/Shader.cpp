#include "Shader.hpp"

#include <sstream>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"

#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <string>

namespace
{
    constexpr std::array<const char*, 4> ShaderTypesString = { "Vertex", "Fragment", "Compute", "Geometry" };

    struct ShaderCompileContext
    {
        Pine::Graphics::ShaderType Type{};
        Pine::Graphics::IShaderProgram* Program{};
        Pine::Shader* Shader{};

        std::string FilePath;
        nlohmann::json Json;

        std::vector<std::string> VersionMacros;
    };

    std::string ProcessShaderLine(ShaderCompileContext& shaderCompileContext, const std::string& line)
    {
        /*
        const auto shaderTypeStr = ShaderTypesString[static_cast<int>(shaderCompileContext.Type)];
        const auto hasParentShader = !shaderCompileContext.Shader->IsBaseShader();
        const auto parentShader = shaderCompileContext.Shader->GetParentShader();

        if (hasParentShader)
        {
            bool isPreHook = Pine::String::StartsWith(line, "#shader pre");
            bool isPostHook = Pine::String::StartsWith(line, "#shader post");

            if (Pine::String::StartsWith(line, "#shader hooks"))
            {
                return Pine::File::ReadFile(parentShader->GetShaderSourceFile(shaderCompileContext.Type).value()).value();
            }

            if (shaderCompileContext.Json.contains("hooks") && (isPreHook || isPostHook))
            {
                std::string hookName = line.substr(isPreHook ? 11 : 12);

                // TODO: implement this crap whenever i feel like it (and it's actually needed)
            }
        }
        */

        if (Pine::String::StartsWith(line, "#include "))
        {
            std::string fileName = Pine::String::Replace(line.substr(9), "\"", "");
            std::filesystem::path filePath = shaderCompileContext.Shader->GetFilePath().parent_path().string() + "/" + fileName;

            if (std::filesystem::exists(filePath))
            {
                auto includedFile = Pine::File::ReadFile(filePath).value();

                std::istringstream stream(includedFile);
                std::ostringstream outputStream;

                std::string includedFileLine;
                while (std::getline(stream, includedFileLine))
                {
                    outputStream << ProcessShaderLine(shaderCompileContext, includedFileLine) << std::endl;
                }

                return outputStream.str();
            }
            else
            {
                Pine::Log::Warning(fmt::format("Failed to find shader include file {} compiling {}", filePath.string(), shaderCompileContext.Shader->GetFileName()));
                return "";
            }
        }

        return line;
    }

    bool LoadAndCompileShader(ShaderCompileContext& shaderCompileContext)
    {
        assert(ShaderTypesString.size() == static_cast<int>(Pine::Graphics::ShaderType::ShaderTypeCount));

        const auto shaderTypeStr = ShaderTypesString[static_cast<int>(shaderCompileContext.Type)];
        const auto hasParentShader = !shaderCompileContext.Shader->IsBaseShader();
        const auto parentShader = shaderCompileContext.Shader->GetParentShader();

        if (!std::filesystem::exists(shaderCompileContext.FilePath) && !hasParentShader)
        {
            Pine::Log::Error("Failed to find shader file " + shaderCompileContext.FilePath);
            return false;
        }

        const std::string file = Pine::File::ReadFile(shaderCompileContext.FilePath).value_or("");

        if (file.empty() && !hasParentShader)
        {
            Pine::Log::Error("Failed to read file " + shaderCompileContext.FilePath);
            return false;
        }

        std::istringstream stream(file);
        std::ostringstream outputStream;
        std::string line;

        while (std::getline(stream, line))
        {
            outputStream << ProcessShaderLine(shaderCompileContext, line) << std::endl;
        }

        std::string src = outputStream.str();

        // Remove remaining stuff from the source.
        src = Pine::String::Replace(src, fmt::format("#shader pre{}", shaderTypeStr), "");
        src = Pine::String::Replace(src, fmt::format("#shader post{}", shaderTypeStr), "");
        src = Pine::String::Replace(src, "#shader hooks", "");

        // Insert any macros for pre-processor if we have to
        if (!shaderCompileContext.VersionMacros.empty())
        {
            // Since we need to add the macros after the version, find that first
            const auto offset = src.find('\n') + 1;

            for (const auto& macro : shaderCompileContext.VersionMacros)
            {
                src = src.insert(offset, fmt::format("#define {}\n", macro));
            }
        }

        if (!shaderCompileContext.Program->CompileAndLoadShader(src, shaderCompileContext.Type))
        {
            Pine::Log::Error(fmt::format("Error occurred in file {}", shaderCompileContext.FilePath));

            return false;
        }

        return true;
    }

    // Gets asset's parent directory and adds that to the path specified
    // in the JSON file. We do this because the file path specified in the JSON
    // is relative to the .shader file, and we need the path relative to the application
    std::string GetAbsolutePath(const Pine::Shader* shader, const std::string& filePath, bool useEnginePath = false)
    {
        const auto path = useEnginePath ? std::filesystem::path(shader->GetPath()) : shader->GetFilePath();
        const auto assetParentDirectory = path.parent_path().string();

        return assetParentDirectory + "/" + filePath;
    }

}

Pine::Shader::Shader()
{
    m_Type = AssetType::Shader;
    m_LoadMode = AssetLoadMode::SingleThread;

    // A shader will always have dependencies, as it will have source files
    m_HasDependencies = true;
}

bool Pine::Shader::LoadFromFile(AssetLoadStage stage)
{
    // Attempt to parse JSON
    const auto jsonOpt = SerializationJson::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
        return false;

    const auto& j = jsonOpt.value();

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
    for (const auto shaderProgram : m_ShaderPrograms)
    {
        Graphics::GetGraphicsAPI()->DestroyShaderProgram(shaderProgram);
    }

    m_ShaderPrograms.clear();
    m_ShaderProgramsReady.clear();
    m_ShaderFiles.clear();

    m_State = AssetState::Unloaded;
}

Pine::Graphics::IShaderProgram* Pine::Shader::GetProgram(ShaderVersion version) const
{
    // surely the user has called HasShaderVersion(...) beforehand and this won't ever crash
    return m_ShaderPrograms[m_ShaderVersionsMap.at(static_cast<std::uint32_t>(version))];
}

bool Pine::Shader::HasBeenUpdated() const
{
    if (Asset::HasBeenUpdated())
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
    Asset::MarkAsUpdated();

    for (const auto& shaderFile : m_ShaderFiles)
    {
        shaderFile->MarkAsUpdated();
    }
}

void Pine::Shader::SetReady(bool ready, ShaderVersion version)
{
    if (!HasShaderVersion(version))
    {
        return;
    }

    m_ShaderProgramsReady[m_ShaderVersionsMap[static_cast<std::uint32_t>(version)]] = ready;
}

bool Pine::Shader::IsReady(ShaderVersion version)
{
    if (!HasShaderVersion(version))
    {
        return m_ShaderProgramsReady[0];
    }

    return m_ShaderProgramsReady[m_ShaderVersionsMap[static_cast<std::uint32_t>(version)]];
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
    auto jsonOpt = SerializationJson::LoadFromFile(m_FilePath);

    if (!jsonOpt.has_value())
        return std::nullopt;

    const auto j = jsonOpt.value();
    const auto shaderTypeString = String::ToLower(ShaderTypesString[static_cast<int>(type)]);

    if (!j.contains(shaderTypeString))
        return std::nullopt;

    return std::make_optional(GetAbsolutePath(this, j[shaderTypeString]));
}

bool Pine::Shader::LoadShaderPackage(const nlohmann::json &j, std::uint32_t shaderVersion, const std::vector<std::string>& versionMacros)
{
    // Get all shader file paths
    std::string vertexPath, fragmentPath, computePath, geometryPath;

    if (j.contains("vertex"))
        vertexPath = j["vertex"];
    if (j.contains("fragment"))
        fragmentPath = j["fragment"];
    if (j.contains("compute"))
        vertexPath = j["compute"];
    if (j.contains("geometry"))
        geometryPath = j["geometry"];

    // Invalid shader configuration
    if (vertexPath.empty() && fragmentPath.empty() && computePath.empty() && geometryPath.empty())
    {
        Log::Error("No shader files specified in " + m_FileName);

        return false;
    }

    // Prepare a graphics shader program
    const auto shaderProgram = Graphics::GetGraphicsAPI()->CreateShaderProgram();

    if (j.contains("parent"))
    {
        m_ParentShader = Pine::Assets::Get<Shader>(j["parent"]);
        m_BaseShader = false;

        if (!m_ParentShader)
        {
            Log::Error(fmt::format("Failed to find parent shader in {}, parent shader: {}.", m_FileName, j["parent"].get<std::string>()));
            return false;
        }
    }

    ShaderCompileContext shaderCompileContext;

    shaderCompileContext.Program = shaderProgram;
    shaderCompileContext.Shader = this;
    shaderCompileContext.VersionMacros = versionMacros;
    shaderCompileContext.Json = j;

    // Load and compile all specified shaders
    if (!vertexPath.empty())
    {
        shaderCompileContext.FilePath = GetAbsolutePath(this, vertexPath);
        shaderCompileContext.Type = Graphics::ShaderType::Vertex;

        if (!LoadAndCompileShader(shaderCompileContext))
        {
            return false;
        }

        if (versionMacros.empty())
        {
            m_ShaderFiles.push_back(Assets::Get(GetAbsolutePath(this, vertexPath, true)));
        }
    }

    if (!fragmentPath.empty())
    {
        shaderCompileContext.FilePath = GetAbsolutePath(this, fragmentPath);
        shaderCompileContext.Type = Graphics::ShaderType::Fragment;

        if (!LoadAndCompileShader(shaderCompileContext))
        {
            return false;
        }

        if (versionMacros.empty())
        {
            m_ShaderFiles.push_back(Assets::Get(GetAbsolutePath(this, fragmentPath, true)));
        }
    }

    if (!computePath.empty())
    {
        shaderCompileContext.FilePath = GetAbsolutePath(this, computePath);
        shaderCompileContext.Type = Graphics::ShaderType::Compute;

        if (!LoadAndCompileShader(shaderCompileContext))
        {
            return false;
        }

        if (versionMacros.empty())
        {
            m_ShaderFiles.push_back(Assets::Get(GetAbsolutePath(this, computePath, true)));
        }
    }

    if (!geometryPath.empty())
    {
        shaderCompileContext.FilePath = GetAbsolutePath(this, geometryPath);
        shaderCompileContext.Type = Graphics::ShaderType::Geometry;

        if (!LoadAndCompileShader(shaderCompileContext))
        {
            return false;
        }

        if (versionMacros.empty())
        {
            m_ShaderFiles.push_back(Assets::Get(GetAbsolutePath(this, geometryPath, true)));
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
            auto parentJson = SerializationJson::LoadFromFile(m_ParentShader->GetFilePath()).value();

            for (const auto& item : parentJson["texture_samplers"].items())
            {
                textureSamplers.emplace_back(item.key(), item.value().get<int>());
            }
        }

        for (const auto& [name, value] : textureSamplers)
        {
            const auto uniformVariable = shaderProgram->GetUniformVariable(name);

            if (uniformVariable == nullptr)
            {
                Log::Warning("Failed to find texture sampler " + name + ", " + m_FileName);
                continue;
            }

            uniformVariable->LoadInteger(value);
        }
    }

    m_ShaderPrograms.push_back(shaderProgram);
    m_ShaderProgramsReady.push_back(false);
    m_ShaderVersionsMap[shaderVersion] = m_ShaderPrograms.size() - 1;

    return true;
}

bool Pine::Shader::HasShaderVersion(ShaderVersion version) const
{
    if (version == ShaderVersion::Default)
    {
        return !m_ShaderPrograms.empty();
    }

    return m_ShaderVersionsMap.count(static_cast<std::uint32_t>(version)) != 0;
}
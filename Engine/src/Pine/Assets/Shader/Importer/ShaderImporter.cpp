#include "ShaderImporter.hpp"

#include <optional>
#include <sstream>
#include <string>

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/String/String.hpp"

namespace
{
    const std::vector<std::pair<std::string, Pine::Graphics::ShaderType>> m_ShaderExtensionMap =
    {
        {".vertex.glsl", Pine::Graphics::ShaderType::Vertex},
        {".vertex", Pine::Graphics::ShaderType::Vertex},

        {".fragment.glsl", Pine::Graphics::ShaderType::Fragment},
        {".fragment", Pine::Graphics::ShaderType::Fragment},

        {".geometry.glsl", Pine::Graphics::ShaderType::Geometry},
        {".geometry", Pine::Graphics::ShaderType::Geometry},

        {".compute.glsl", Pine::Graphics::ShaderType::Compute},
        {".compute", Pine::Graphics::ShaderType::Compute},
    };

    std::optional<Pine::Graphics::ShaderType> GetShaderType(const std::string& shaderFilePath)
    {
        bool foundShaderType = false;
        Pine::Graphics::ShaderType shaderType;

        for (const auto& [typeExtension, type] : m_ShaderExtensionMap)
        {
            if (Pine::String::EndsWith(shaderFilePath, typeExtension))
            {
                shaderType = type;
                foundShaderType = true;
                break;
            }
        }

        if (!foundShaderType)
        {
            return std::nullopt;
        }

        return shaderType;
    }
}

std::string Pine::Importer::ShaderImporter::ProcessShaderLine(Shader* shader, const std::string& line)
{
    if (String::StartsWith(line, "#shader "))
    {
        return "";
    }

    if (String::StartsWith(line, "#include "))
    {
        // Figure out included file path
        std::string fileName = String::Replace(line.substr(9), "\"", "");
        std::filesystem::path filePath = shader->GetFilePath().parent_path().string() + "/" + fileName;

        if (!std::filesystem::exists(filePath))
        {
            Log::Warning(fmt::format("Could not find file in include directive: {}", fileName));
            return "";
        }

        // Process included file using recursion
        std::istringstream inputStream(File::ReadFile(filePath).value_or(""));
        std::ostringstream outputStream;

        std::string includedFileLine;
        while (std::getline(inputStream, includedFileLine))
        {
            outputStream << ProcessShaderLine(shader, String::Trim(includedFileLine)) << std::endl;
        }

        return outputStream.str();
    }

    return line;
}

std::optional<std::string> Pine::Importer::ShaderImporter::ProcessShaderSource(
    Shader* shader,
    const std::string& filePath)
{
    std::string file = File::ReadFile(filePath).value_or("");

    if (file.empty())
    {
        Log::Error("Failed to read file " + filePath);
        return std::nullopt;
    }

    std::istringstream inputStream(file);
    std::ostringstream outputStream;

    std::string line;
    while (std::getline(inputStream, line))
    {
        outputStream << ProcessShaderLine(shader, String::Trim(line)) << std::endl;
    }

    return outputStream.str();
}

bool Pine::Importer::ShaderImporter::Import(Shader* shader)
{
    if (shader->m_SourceFiles.empty())
    {
        Log::Error(fmt::format("Could not import shader {}, no source files.", shader->m_FilePath.string()));
        return false;
    }

    for (const auto& [sourceFilePath,_] : shader->m_SourceFiles)
    {
        auto type = GetShaderType(sourceFilePath);

        if (!type.has_value())
        {
            Log::Warning(fmt::format("Ignoring file {}, could not determine shader type", sourceFilePath));
            continue;
        }

        const auto processedSource = ProcessShaderSource(shader, sourceFilePath);
        if (!processedSource.has_value())
        {
            continue;
        }

        shader->m_ShaderSources[static_cast<uint32_t>(type.value())] = processedSource.value();
    }

    return true;
}
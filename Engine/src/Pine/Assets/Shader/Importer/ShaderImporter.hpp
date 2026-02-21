#pragma once
#include <optional>

#include "Pine/Assets/Shader/Shader.hpp"

namespace Pine::Importer
{

    class ShaderImporter
    {
    private:
        static std::string ProcessShaderLine(Shader* shader, const std::string& line);
        static std::optional<std::string> ProcessShaderSource(Shader* shader, const std::string& filePath);
    public:
        static bool Import(Shader* shader);
    };

}

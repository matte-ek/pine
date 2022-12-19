#pragma once
#include "IUniformBuffer.hpp"
#include "IUniformVariable.hpp"
#include <string>

namespace Pine::Graphics
{

    enum class ShaderType
    {
        Vertex,
        Fragment,
        Compute
    };

    class IShaderProgram
    {
    public:
        virtual ~IShaderProgram() = default;

        virtual void Use() = 0;
        virtual void Dispose() = 0;

        virtual bool CompileAndLoadShader(const std::string& src, ShaderType type) = 0;
        virtual bool LinkProgram() = 0;

        virtual IUniformVariable* GetUniformVariable(const std::string& name) = 0;

        virtual bool AttachUniformBuffer(IUniformBuffer* buffer, const std::string& bufferName) = 0;
    };

} // namespace Pine::Graphics
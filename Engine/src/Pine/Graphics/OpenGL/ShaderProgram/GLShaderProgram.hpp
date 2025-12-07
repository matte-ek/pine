#pragma once
#include <vector>
#include <unordered_map>

#include "Pine/Graphics/Interfaces/IShaderProgram.hpp"
#include "Pine/Graphics/OpenGL/UniformVariable/GLUniformVariable.hpp"

namespace Pine::Graphics
{

    // OpenGL implementation of IShaderProgram
    class GLShaderProgram : public IShaderProgram
    {
    private:
        std::uint32_t m_Id = 0;

        std::vector<std::uint32_t> m_CompiledShaders;

        std::unordered_map<std::string, GLUniformVariable*> m_UniformVariables;
    public:

        void Use() override;
        void Dispose() override;

        bool CompileAndLoadShader(const std::string& src, ShaderType type) override;
        bool LinkProgram() override;

        IUniformVariable* GetUniformVariable(const std::string& name) override;

        bool AttachUniformBuffer(IUniformBuffer* buffer, const std::string& bufferName) override;

        static void ResetChangeTracking();
    };

}
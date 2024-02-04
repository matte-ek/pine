#include "GLShaderProgram.hpp"
#include "Pine/Core/Log/Log.hpp"
#include <GL/glew.h>
#include <stdexcept>

namespace
{
    std::uint32_t m_ActiveShader = 0;

    std::uint32_t TranslateShaderType(Pine::Graphics::ShaderType type)
    {
        switch (type)
        {
            case Pine::Graphics::ShaderType::Vertex:
                return GL_VERTEX_SHADER;
            case Pine::Graphics::ShaderType::Fragment:
                return GL_FRAGMENT_SHADER;
            case Pine::Graphics::ShaderType::Compute:
                return GL_COMPUTE_SHADER;
            default:
                throw std::runtime_error("Unsupported shader type.");
        }
    }
}

void Pine::Graphics::GLShaderProgram::Use()
{
    if (m_ActiveShader == m_Id)
    {
        return;
    }

    glUseProgram(m_Id);

    m_ActiveShader = m_Id;
}

void Pine::Graphics::GLShaderProgram::Dispose()
{
    for (const auto &shader: m_CompiledShaders)
    {
        glDeleteShader(shader);
    }

    if (m_Id != 0)
    {
        glDeleteProgram(m_Id);
    }
}

bool Pine::Graphics::GLShaderProgram::CompileAndLoadShader(const std::string &src, ShaderType type)
{
    const auto openglShaderType = TranslateShaderType(type);

    // Create and compile the shader
    const int32_t shader = glCreateShader(openglShaderType);

    const auto src_c_str = src.c_str();
    glShaderSource(shader, 1, &src_c_str, nullptr);

    glCompileShader(shader);

    // Query the compile status of the shader, so we know if anything went wrong.
    int32_t compileStatus = 0;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

    if (compileStatus == GL_FALSE)
    {
        // Get the error message length
        int32_t errorStrLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &errorStrLength);

        // Get the error message itself
        std::vector<char> errorMessageArray(errorStrLength);
        glGetShaderInfoLog(shader, errorStrLength, &errorStrLength, &errorMessageArray[0]);

        // Get a char array pointer from the vector, since it has a
        // NULL character, this will work fine.
        const char *errorMessage = errorMessageArray.data();

        // Just printing the error should be fine for now
        Log::Error(errorMessage);

        // Let's not forget to remove the shader as well
        glDeleteShader(shader);

        return false;
    }

    m_CompiledShaders.push_back(shader);

    return true;
}

bool Pine::Graphics::GLShaderProgram::LinkProgram()
{
    if (m_CompiledShaders.empty())
    {
        Log::Error("No compiled shaders when linking program.");
        return false;
    }

    // Create the shader program then attach our newly compiled shaders.

    m_Id = glCreateProgram();

    for (const auto &shader: m_CompiledShaders)
    {
        glAttachShader(m_Id, shader);
    }

    glLinkProgram(m_Id);

    // Again, we want to query the status to detect any errors
    int32_t linkStatus = 0;

    glGetProgramiv(m_Id, GL_LINK_STATUS, &linkStatus);

    if (linkStatus == GL_FALSE)
    {
        // Get the error message length
        int32_t errorStrLength = 0;
        glGetProgramiv(m_Id, GL_INFO_LOG_LENGTH, &errorStrLength);

        // Get the error message itself
        std::vector<char> errorMessageArray(errorStrLength);
        glGetProgramInfoLog(m_Id, errorStrLength, &errorStrLength, &errorMessageArray[0]);

        // Get a char array pointer from the vector, since it has a
        // NULL character, this will work fine.
        const char *errorMessage = errorMessageArray.data();

        // Just printing the error should be fine for now
        Log::Error(errorMessage);

        return false;
    }

    // Since the link was successful, we can now safely detach and remove the shaders.
    for (auto shader: m_CompiledShaders)
    {
        glDetachShader(m_Id, shader);
        glDeleteShader(shader);
    }

    m_CompiledShaders.clear();

    return true;
}

Pine::Graphics::IUniformVariable *Pine::Graphics::GLShaderProgram::GetUniformVariable(const std::string &name)
{
    if (m_UniformVariables.count(name) == 0)
    {
        // Attempt to find the variable, and create an GLUniformVariable object if we do
        const auto uniformLocation = glGetUniformLocation(m_Id, name.c_str());

        if (uniformLocation >= 0)
        {
            m_UniformVariables[name] = new GLUniformVariable(uniformLocation);
        }
        else
        {
            Log::Warning("Failed to find uniform variable: " + name);

            return nullptr;
        }
    }

    return m_UniformVariables[name];
}

bool Pine::Graphics::GLShaderProgram::AttachUniformBuffer(IUniformBuffer*buffer,
                                                          const std::string &bufferName)
{
    const int bufferIndex = glGetUniformBlockIndex(m_Id, bufferName.c_str());

    if (0 > bufferIndex)
    {
        Log::Error(fmt::format("Failed to find uniform buffer {}", bufferName));

        return false;
    }

    glUniformBlockBinding(m_Id, bufferIndex, buffer->GetBindIndex());

    return true;
}
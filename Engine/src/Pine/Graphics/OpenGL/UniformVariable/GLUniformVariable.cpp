#include "GLUniformVariable.hpp"
#include <GL/glew.h>

Pine::Graphics::GLUniformVariable::GLUniformVariable(std::int32_t id) :
      m_Id(id)
{
}

void Pine::Graphics::GLUniformVariable::LoadInteger(int value)
{
    glUniform1i(m_Id, value);
}

void Pine::Graphics::GLUniformVariable::LoadFloat(float value)
{
    glUniform1f(m_Id, value);
}

void Pine::Graphics::GLUniformVariable::LoadVector2(const Vector2f& value)
{
    glUniform2f(m_Id, value.x, value.y);
}

void Pine::Graphics::GLUniformVariable::LoadVector3(const Vector3f& value)
{
    glUniform3f(m_Id, value.x, value.y, value.z);
}

void Pine::Graphics::GLUniformVariable::LoadVector4(const Vector4f& value)
{
    glUniform4f(m_Id, value.x, value.y, value.z, value.w);
}

void Pine::Graphics::GLUniformVariable::LoadVector2(const Vector2i& value)
{
    glUniform2i(m_Id, value.x, value.y);
}

void Pine::Graphics::GLUniformVariable::LoadVector3(const Vector3i& value)
{
    glUniform3i(m_Id, value.x, value.y, value.z);
}

void Pine::Graphics::GLUniformVariable::LoadVector4(const Vector4i& value)
{
    glUniform4i(m_Id, value.x, value.y, value.z, value.w);
}

void Pine::Graphics::GLUniformVariable::LoadMatrix4(const Matrix4f& value)
{
    glUniformMatrix4fv(m_Id, 1, false, &value[0][0]);
}
#pragma once

#include <utility>

#include "Pine/Graphics/Interfaces/IUniformBuffer.hpp"
#include "Pine/Graphics/Graphics.hpp"

namespace Pine::Graphics
{


    // Abstraction for either a UBO or SSBO (in the future)
    template<typename T>
    class ShaderStorage
    {
    private:
        IUniformBuffer* m_UniformBuffer = nullptr;

        std::string m_Name;
        int m_BindIndex = 0;

        T m_Data;
    public:
        explicit ShaderStorage(int index, std::string name);

        void Create();
        void Dispose();

        bool AttachShader(Shader* shader);

        void Upload(size_t size = 0);

        int BindIndex() const;
        T& Data();
    };

    template<typename T>
    ShaderStorage<T>::ShaderStorage(int index, std::string name)
        : m_BindIndex(index),
        m_Name(std::move(name))
    {
    }

    template<typename T>
    bool ShaderStorage<T>::AttachShader(Shader*shader)
    {
        return shader->GetProgram()->AttachUniformBuffer(m_UniformBuffer, m_Name);
    }

    template<typename T>
    int ShaderStorage<T>::BindIndex() const
    {
        return m_BindIndex;
    }

    template<typename T>
    T &ShaderStorage<T>::Data()
    {
        return m_Data;
    }

    template<typename T>
    void ShaderStorage<T>::Upload(size_t size)
    {
        m_UniformBuffer->Bind();

        m_UniformBuffer->UploadData(&m_Data, size == 0 ? sizeof(T) : size, 0);
    }

    template<typename T>
    void ShaderStorage<T>::Dispose()
    {
        m_UniformBuffer->Dispose();

        m_UniformBuffer = nullptr;
    }

    template<typename T>
    void ShaderStorage<T>::Create()
    {
        m_UniformBuffer = GetGraphicsAPI()->CreateUniformBuffer();
        m_UniformBuffer->Create(sizeof(T), m_BindIndex);
    }

}
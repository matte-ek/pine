#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "../../../../../Editor/include/IconsMaterialDesign.h"
#include "Pine/Core/Math/Math.hpp"

namespace Pine::Serialization
{
    enum class SerializationType
    {
        Flexible,
        Compact
    };

    enum class DataType : std::uint8_t
    {
        Invalid = 0,

        // Known size
        Int32,
        Float32,
        Vec2,
        Vec3,
        Vec4,
        Quaternion,

        // Unknown size
        String,
        Asset,
        Data,

        Count
    };

    class Serializer;

    class Data
    {
    protected:
        const char* m_Name;

        bool m_IsArray = false;
        DataType m_Type;
        size_t m_DataSize;

        char m_Data[16] {};
        void* m_VariableData = nullptr;
    public:
        Data(
            Serializer* parentSerializer,
            DataType type,
            const char* name,
            size_t size = 0);

        ~Data();

        template <typename T>
        T Read();

        const char* ReadString() const;

        template <typename T>
        void Write(const T& data);

        void WriteString(const char* str);
        void WriteString(const std::string& str);

        void* GetData() const;
        size_t GetDataSize() const;

        friend class Serializer;
    };

    template<typename T>
    T Data::Read()
    {
        if (static_cast<std::uint8_t>(m_Type) < static_cast<std::uint8_t>(DataType::String))
        {
            return *reinterpret_cast<T*>(m_Data);
        }
        else
        {
            return T();
        }
    }

    template<typename T>
    void Data::Write(const T& data)
    {
        m_DataSize = sizeof(T);

        if (static_cast<std::uint8_t>(m_Type) < static_cast<std::uint8_t>(DataType::String))
        {
            if (sizeof(T) > sizeof(m_Data))
            {
                m_VariableData = malloc(m_DataSize);
                memcpy(m_VariableData, &data, sizeof(T));
                return;
            }

            memcpy(m_Data, &data, sizeof(T));
        }
        else
        {
            m_VariableData = malloc(m_DataSize);
            memcpy(m_VariableData, &data, m_DataSize);
        }
    }

    class Serializer
    {
    protected:
        std::vector<Data*> m_Data;
    public:
        bool Read(void* data, size_t size) const;
        void* Write(size_t& outputSize) const;

        friend class Data;
    };
}

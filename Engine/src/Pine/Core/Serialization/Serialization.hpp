#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "Pine/Core/UId/UId.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Span/Span.hpp"
#include "Pine/Input/Input.hpp"

namespace Pine
{
    template<class T>
    class AssetHandle;
}

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
        Boolean,
        Int32,
        Int64,
        Float32,
        Vec2,
        Vec3,
        Vec4,
        Quaternion,
        UId,

        // Unknown size
        String,
        Data,
        Array,

        Count
    };

    class Serializer;

    class Data
    {
    private:
        const char* m_Name;
        DataType m_Type;
    public:
        Data(
            Serializer* parentSerializer,
            DataType type,
            const char* name);

        virtual ~Data() = default;

        const char* GetName() const;
        DataType GetType() const;

        virtual size_t GetDataSize() const = 0;
    };

    template<typename>
    struct IsAssetHandle : std::false_type {};

    template<typename T>
    struct IsAssetHandle<AssetHandle<T>> : std::true_type {};

    template<typename T>
    constexpr bool IsAssetHandle_v = IsAssetHandle<T>::value;

    // Data storage used for types smaller than 16 bytes
    class DataPrimitive final : public Data
    {
    private:
        char m_Data[16];
        size_t m_DataSize;
    protected:
        void Write(const void* data, size_t size);

        const void* GetData() const;
        size_t GetDataSize() const override;
    public:
        DataPrimitive(Serializer* parentSerializer, DataType type, const char* name);

        template<typename TAsset>
        bool Read(AssetHandle<TAsset>& handle)
        {
            assert(m_DataSize == sizeof(UId));

            handle = AssetHandle<TAsset>(*static_cast<const UId*>(GetData()));

            return true;
        }

        template<typename TPrimitive>
        bool Read(TPrimitive& data)
        {
            if (sizeof(TPrimitive) != m_DataSize)
            {
                Pine::Log::Warning("Failed to write primitive type, size is mismatched. Is the type correct?");
                return false;
            }

            data = *static_cast<const TPrimitive*>(GetData());

            return true;
        }

        template<typename TPrimitive>
        TPrimitive Read()
        {
            assert(sizeof(TPrimitive) == m_DataSize);

            return *static_cast<const TPrimitive*>(GetData());
        }

        template<typename TAsset>
        bool Write(const AssetHandle<TAsset>& handle)
        {
            assert(m_DataSize == sizeof(UId));

            memcpy(m_Data, &handle, sizeof(UId));

            return true;
        }

        template<typename TPrimitive>
        bool Write(const TPrimitive& data)
        {
            assert(sizeof(TPrimitive) == m_DataSize);

            memcpy(m_Data, &data, sizeof(TPrimitive));

            return true;
        }

        friend class Serializer;
    };

    // Data storage used for types larger than 16 bytes.
    class DataFixed : public Data
    {
    private:
        void* m_Data;
        size_t m_DataSize;
    protected:
        void AllocateData(size_t size);

        const void* GetData() const;
        size_t GetDataSize() const override;
    public:
        DataFixed(Serializer* parentSerializer, DataType type, const char* name);
        ~DataFixed() override;

        bool ReadRaw(void** data, size_t& size) const;
        void WriteRaw(const void* data, size_t size);
        void WritePortion(const void* data, size_t size, size_t offset) const;

        ByteSpan Read() const;

        bool Read(std::string& str) const;
        void Write(const std::string& str);

        bool Read(ByteSpan& span) const;
        void Write(const ByteSpan& span);

        friend class Serializer;
    };

    // Data storage used for "list" arrays, any element can be any size. Somewhat inefficient for smaller elements.
    class DataArray final : public Data
    {
    private:
        std::vector<ByteSpan> m_Data;
    public:
        DataArray(Serializer* parentSerializer, const char* name);
        ~DataArray() override;

        void Reset();

        void AddData(const void* data, size_t size);
        void AddData(const ByteSpan& span);

        const ByteSpan& GetData(size_t index) const;

        size_t GetDataCount() const;
        size_t GetDataSize() const override;

        friend class Serializer;
    };

    // Data storage for arrays with fixed storage elements.
    class DataArrayFixed final : protected DataFixed
    {
    private:
        size_t m_DataStride;
    public:
        DataArrayFixed(Serializer* parentSerializer, const char* name, size_t elementSize);

        template<typename TElement, size_t TSize>
        void Read(std::array<TElement, TSize>& data)
        {
            if (GetDataCount() != TSize || m_DataStride != sizeof(TElement))
            {
                return;
            }

            memcpy(data.data(), GetData(), GetDataSize());
        }

        template<typename TElement, size_t TSize>
        void Write(const std::array<TElement, TSize>& data)
        {
            if (m_DataStride != sizeof(TElement))
            {
                return;
            }

            WriteRaw(data.data(), data.size() * sizeof(TElement));
        }

        template<typename TElement>
        void Read(std::vector<TElement>& vec)
        {
            if (m_DataStride != sizeof(TElement))
            {
                return;
            }

            vec.clear();
            vec.resize(GetDataCount());

            memcpy(vec.data(), GetData(), GetDataSize());
        }

        template<typename TElement>
        void Write(const std::vector<TElement>& vec)
        {
            if (m_DataStride != sizeof(TElement))
            {
                return;
            }

            WriteRaw(vec.data(), vec.size() * sizeof(TElement));
        }

        template<typename TElement>
        TElement ReadElement(uint32_t elementId)
        {
            assert(elementId < GetDataCount() && m_DataStride == sizeof(TElement));

            TElement ret{};

            memcpy(&ret, static_cast<const char*>(GetData()) + sizeof(TElement) * elementId, sizeof(TElement));

            return ret;
        }

        template<typename TElement>
        void WriteElement(uint32_t elementId, const TElement& data)
        {
            assert(elementId < GetDataCount() && m_DataStride == sizeof(TElement));

            WritePortion(&data, sizeof(TElement), sizeof(TElement) * elementId);
        }

        void SetSize(uint32_t size);

        std::uint32_t GetDataCount() const;
    };

    class Serializer
    {
    protected:
        std::vector<Data*> m_Data;
    public:
        Serializer() = default;
        ~Serializer();

        bool Read(const std::filesystem::path& path) const;
        bool Write(const std::filesystem::path& path) const;

        bool Read(const ByteSpan& span) const;
        ByteSpan Write() const;

        bool Read(const void* data, size_t size) const;
        std::byte* Write(size_t& outputSize) const;

        using Primitive  = DataPrimitive;
        using Fixed      = DataFixed;
        using Array      = DataArray;
        using ArrayFixed = DataArrayFixed;

        friend class Data;
    };
}

#define PINE_SERIALIZE_PRIMITIVE(str, type) static_assert(static_cast<int>(type) < static_cast<int>(Pine::Serialization::DataType::String)); Pine::Serialization::DataPrimitive str = Pine::Serialization::DataPrimitive(this, type, #str)

#define PINE_SERIALIZE_DATA(str) Pine::Serialization::DataFixed str = Pine::Serialization::DataFixed(this, Pine::Serialization::DataType::Data, #str)
#define PINE_SERIALIZE_STRING(str) Pine::Serialization::DataFixed str = Pine::Serialization::DataFixed(this, Pine::Serialization::DataType::String, #str)
#define PINE_SERIALIZE_ASSET(str) Pine::Serialization::DataPrimitive str = Pine::Serialization::DataPrimitive(this, Pine::Serialization::DataType::UId, #str)

#define PINE_SERIALIZE_ARRAY(str) Pine::Serialization::DataArray str = Pine::Serialization::DataArray(this, #str)
#define PINE_SERIALIZE_ARRAY_FIXED(str, type) Pine::Serialization::DataArrayFixed str = Pine::Serialization::DataArrayFixed(this, #str, sizeof(type))
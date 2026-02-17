#include "Serialization.hpp"

#include <fstream>

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"

namespace
{
    constexpr std::uint32_t PINE_MAGIC = 0x7143;
    constexpr std::uint16_t PINE_VERSION = 0x1;

    constexpr bool PINE_COMPACT_MODE = false;

    enum class FileHeaderFlags : std::uint16_t
    {
        FlexibleMode = (1 << 0) // File was encoded with flexible mode enabled.
    };

    size_t PrimitiveDataTypeToSize(Pine::Serialization::DataType type)
    {
        switch (type)
        {
        case Pine::Serialization::DataType::Boolean:
            return sizeof(bool);
        case Pine::Serialization::DataType::Int32:
            return sizeof(std::int32_t);
        case Pine::Serialization::DataType::Int64:
            return sizeof(std::int64_t);
        case Pine::Serialization::DataType::Float32:
            return sizeof(float);
        case Pine::Serialization::DataType::Vec2:
            return sizeof(Pine::Vector2f);
        case Pine::Serialization::DataType::Vec3:
            return sizeof(Pine::Vector3f);
        case Pine::Serialization::DataType::Vec4:
            return sizeof(Pine::Vector4f);
        case Pine::Serialization::DataType::Quaternion:
            return sizeof(Pine::Quaternion);
        case Pine::Serialization::DataType::Guid:
            return sizeof(Pine::Guid);
        default:
            throw new std::logic_error("Data type not primitive or invalid.");
        }
    }

#pragma pack(push, 1)
    struct FileHeader
    {
        std::uint32_t Magic;

        // The version number of this file, if the file was encoded
        // in compact mode, this has to match, otherwise it's probably fine.
        std::uint16_t Version;

        // Optional flags for this file
        std::uint8_t Flags;

        // The amount of data fields in this file
        std::uint16_t DataCount;
    };

    struct DataHeader
    {
        std::uint8_t Type;
    };

    struct DataHeaderFlexible : DataHeader
    {
        std::uint8_t DataNameLength;
        char* DataName;
    };
#pragma pack(pop)
}

Pine::Serialization::Data::Data(
    Serializer* parentSerializer,
    DataType type,
    const char* name)
    :
    m_Name(name),
    m_Type(type)
{
    parentSerializer->m_Data.push_back(this);
}

const char* Pine::Serialization::Data::GetName() const
{
    return m_Name;
}

Pine::Serialization::DataType Pine::Serialization::Data::GetType() const
{
    return m_Type;
}

void Pine::Serialization::DataPrimitive::Write(const void* data, size_t size)
{
    if (size > sizeof(m_Data))
    {
        // Something has gone very wrong, just bail.
        throw new std::logic_error("Data for primitive size too big.");
    }

    memcpy(m_Data, data, size);

    m_DataSize = size;
}

Pine::Serialization::DataPrimitive::DataPrimitive(Serializer* parentSerializer, DataType type, const char* name)
: Data(parentSerializer, type, name),
  m_Data{}
{
    m_DataSize = PrimitiveDataTypeToSize(type);
}

const void* Pine::Serialization::DataPrimitive::GetData() const
{
    return &m_Data;
}

size_t Pine::Serialization::DataPrimitive::GetDataSize() const
{
    return m_DataSize;
}

void Pine::Serialization::DataFixed::WriteRaw(const void* data, size_t size)
{
    if (size == 0)
    {
        return;
    }

    m_Data = malloc(size);

    memcpy(m_Data, data, size);

    m_DataSize = size;
}

Pine::ByteSpan Pine::Serialization::DataFixed::Read() const
{
    if (m_DataSize == 0)
    {
        return {};
    }

    ByteSpan span;

    span.data = new std::byte[m_DataSize];
    span.size = m_DataSize;

    memcpy(span.data, m_Data, m_DataSize);

    return span;
}

bool Pine::Serialization::DataFixed::Read(std::string& str) const
{
    if (m_DataSize == 0)
    {
        return false;
    }

    str = std::string(static_cast<char*>(m_Data), m_DataSize);

    return true;
}

void Pine::Serialization::DataFixed::Write(const std::string& str)
{
    WriteRaw(str.c_str(), str.size());
}

bool Pine::Serialization::DataFixed::Read(ByteSpan& span) const
{
    if (m_DataSize == 0)
    {
        return false;
    }

    span.data = new std::byte[m_DataSize];
    span.size = m_DataSize;

    memcpy(span.data, m_Data, m_DataSize);

    return true;
}

void Pine::Serialization::DataFixed::Write(const ByteSpan& span)
{
    WriteRaw(span.data, span.size);
}

Pine::Serialization::DataFixed::DataFixed(Serializer* parentSerializer, DataType type, const char* name)
: Data(parentSerializer, type, name),
  m_Data{}, m_DataSize{}
{
}

Pine::Serialization::DataFixed::~DataFixed()
{
    if (m_Data)
    {
        free(m_Data);
    }

    m_Data = nullptr;
}

bool Pine::Serialization::DataFixed::ReadRaw(void** data, size_t& size) const
{
    assert(data != nullptr);

    if (m_DataSize == 0)
    {
        Pine::Log::Error("Failed to load fixed data, size is empty.");
        return false;
    }

    *data = malloc(m_DataSize);

    memcpy(*data, m_Data, m_DataSize);

    size = m_DataSize;

    return true;
}

const void* Pine::Serialization::DataFixed::GetData() const
{
    return m_Data;
}

size_t Pine::Serialization::DataFixed::GetDataSize() const
{
    return m_DataSize;
}

Pine::Serialization::DataArray::DataArray(Serializer* parentSerializer, const char* name)
    : Data(parentSerializer, DataType::Array, name)
{
}

Pine::Serialization::DataArray::~DataArray()
{
    m_Data.clear();
}

void Pine::Serialization::DataArray::Reset()
{
    m_Data.clear();
}

void Pine::Serialization::DataArray::AddData(const void* data, size_t size)
{
    ByteSpan span;

    span.data = new std::byte[size];
    span.size = size;

    memcpy(span.data, data, size);

    m_Data.push_back(span);
}

void Pine::Serialization::DataArray::AddData(const ByteSpan& span)
{
    AddData(span.data, span.size);
}

const Pine::ByteSpan& Pine::Serialization::DataArray::GetData(size_t index) const
{
    if (index >= m_Data.size())
    {
        return {nullptr, 0};
    }

    return m_Data[index];
}

size_t Pine::Serialization::DataArray::GetDataCount() const
{
    return m_Data.size();
}

size_t Pine::Serialization::DataArray::GetDataSize() const
{
    size_t ret{};

    for (const auto& [_, Size] : m_Data)
    {
        ret += Size + sizeof(std::uint32_t);
    }

    return ret;
}

Pine::Serialization::DataArrayFixed::DataArrayFixed(Serializer* parentSerializer, const char* name, size_t elementSize)
    : DataFixed(parentSerializer, DataType::Data, name),
        m_DataStride(elementSize)
{
}

std::uint32_t Pine::Serialization::DataArrayFixed::GetDataCount() const
{
    return GetDataSize() / m_DataStride;
}

Pine::Serialization::Serializer::~Serializer()
{
    m_Data.clear();
}

bool Pine::Serialization::Serializer::Read(const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    return Read(File::ReadRaw(path));
}

bool Pine::Serialization::Serializer::Write(const std::filesystem::path& path) const
{
    size_t size;

    auto data = Write(size);

    if (data == nullptr)
    {
        return false;
    }

    std::ofstream file(path, std::ios::out | std::ios::binary);

    file.write(reinterpret_cast<char*>(data), size);
    file.close();

    delete[] data;

    return true;
}

bool Pine::Serialization::Serializer::Read(const ByteSpan& span) const
{
    return Read(span.data, span.size);
}

Pine::ByteSpan Pine::Serialization::Serializer::Write() const
{
    size_t size;

    auto data = Write(size);

    return {data, size};
}

bool Pine::Serialization::Serializer::Read(const void* data, size_t size) const
{
    if (!data || size < sizeof(FileHeader))
    {
        return false;
    }

    auto header = static_cast<const FileHeader*>(data);

    if (header->Magic != PINE_MAGIC)
    {
        return false;
    }

    bool useCompactMode = PINE_COMPACT_MODE;

    if (useCompactMode)
    {
        // If we're in compact mode make sure this file is encoded
        // with the correct version.
        if (header->Version != PINE_VERSION)
        {
            return false;
        }

        if (header->Flags & static_cast<std::uint16_t>(FileHeaderFlags::FlexibleMode))
        {
            useCompactMode = false;
        }
    }
    else
    {
        // If we're in flexible, make sure the file is also in flexible mode,
        // if the version is the same we could probably maybe still deal with it but w/e
        if (!(header->Flags & static_cast<std::uint16_t>(FileHeaderFlags::FlexibleMode)))
        {
            useCompactMode = true;
        }
    }

    const char* dataPtr = static_cast<const char*>(data) + sizeof(FileHeader);
    size_t dataRemaining = size - sizeof(FileHeader);

    for (std::uint32_t i = 0; i < header->DataCount; i++)
    {
        // Make sure we still have data left.
        if (sizeof(DataHeader) > dataRemaining)
        {
            Log::Error("Ran out of data while parsing pine file, probably corrupted.");
            return false;
        }

        auto dataHeader = reinterpret_cast<const DataHeader*>(dataPtr);

        if (dataHeader->Type == 0 || dataHeader->Type >= static_cast<std::uint8_t>(DataType::Count))
        {
            Log::Error("Invalid data type while parsing file, probably corrupted.");
            return false;
        }

        char dataName[32];

        if (!useCompactMode)
        {
            if (sizeof(DataHeaderFlexible) > dataRemaining)
            {
                Log::Error("Ran out of data while parsing pine file, probably corrupted.");
                return false;
            }

            auto flexibleHeader = reinterpret_cast<const DataHeaderFlexible*>(dataPtr);
            auto headerTotalSize = sizeof(DataHeader) + sizeof(std::uint8_t) + (sizeof(char) * flexibleHeader->DataNameLength);

            if (headerTotalSize > dataRemaining)
            {
                Log::Error("Ran out of data while parsing pine file, probably corrupted.");
                return false;
            }

            memcpy(dataName, &flexibleHeader->DataName, flexibleHeader->DataNameLength);

            dataName[flexibleHeader->DataNameLength] = '\0';

            dataPtr += headerTotalSize;
            dataRemaining -= headerTotalSize;
        }
        else
        {
            if (i >= m_Data.size())
            {
                Log::Error("Invalid data index while reading data.");
                return false;
            }

            dataPtr += sizeof(DataHeader);
            dataRemaining -= sizeof(DataHeader);
        }

        size_t dataSize = 0;
        bool dynamicSize = false;

        // These types we can figure out the length of.
        if (dataHeader->Type < static_cast<std::uint8_t>(DataType::String))
        {
            dataSize = PrimitiveDataTypeToSize(static_cast<DataType>(dataHeader->Type));
        }
        else
        {
            if (dataRemaining < sizeof(std::uint32_t))
            {
                Log::Error("Ran out of data while reading data?");
                return false;
            }

            dataSize = *reinterpret_cast<const std::uint32_t*>(dataPtr);

            dataPtr += sizeof(std::uint32_t);
            dataRemaining -= sizeof(std::uint32_t);
            dynamicSize = true;

            // TODO: Remove me, used while testing.
            if (dataSize >= 0x1000000)
            {
                Log::Error("CHECK ME: Size big while reading data");
                return false;
            }
        }

        if (dataSize == 0)
        {
            // To not break alignment, manually add the "count" bytes and move on.
            if (dataHeader->Type == static_cast<int>(DataType::Array))
            {
                dataPtr += sizeof(std::uint32_t);
                dataRemaining -= sizeof(std::uint32_t);
            }

            continue;
        }

        if (dataSize > dataRemaining)
        {
            Log::Error("Ran out of data while reading data?");
            return false;
        }

        Data* dataStructure = nullptr;

        if (useCompactMode)
        {
            dataStructure = m_Data[i];
        }
        else
        {
            for (auto dataCandidate : m_Data)
            {
                if (strcmp(dataCandidate->GetName(), dataName) == 0)
                {
                    dataStructure = dataCandidate;
                    break;
                }
            }
        }

        if (!dataStructure)
        {
            if (useCompactMode)
            {
                Log::Error("Could not find data structure while reading data.");
                return false;
            }
            else
            {
                size_t requiredPadding = dataSize;

                if (dataHeader->Type == static_cast<int>(DataType::Array))
                {
                    dataPtr += sizeof(std::uint32_t);
                }

                dataPtr += requiredPadding;
                dataRemaining -= requiredPadding;

                continue;
            }
        }

        if (static_cast<std::uint8_t>(dataStructure->GetType()) != dataHeader->Type)
        {
            Log::Error("Mismatch data types while reading data.");
            return false;
        }

        if (dataStructure->GetType() == DataType::Array)
        {
            auto arrayData = dynamic_cast<DataArray*>(dataStructure);
            if (!arrayData)
            {
                Log::Error("Data of type array is not DataArray.");
                return false;
            }

            auto arrayCount = *reinterpret_cast<const std::uint32_t*>(dataPtr);

            dataPtr += sizeof(std::uint32_t);
            dataRemaining -= sizeof(std::uint32_t);

            for (size_t j{}; j < arrayCount; j++)
            {
                if (dataRemaining < sizeof(std::uint32_t))
                {
                    Log::Error("Ran out of data while reading array data.");
                    return false;
                }

                auto arrayElementSize = *reinterpret_cast<const std::uint32_t*>(dataPtr);

                dataPtr += sizeof(std::uint32_t);
                dataRemaining -= sizeof(std::uint32_t);

                if (dataRemaining < arrayElementSize)
                {
                    Log::Error("Ran out of data while reading array data.");
                    return false;
                }

                // TODO: Remove me, used while testing.
                if (arrayElementSize >= 0x1000000)
                {
                    Log::Error("CHECK ME: Size big while reading data");
                    return false;
                }

                arrayData->AddData(dataPtr, arrayElementSize);

                dataPtr += arrayElementSize;
                dataRemaining -= arrayElementSize;
            }
        }
        else
        {
            if (!dynamicSize)
            {
                auto dataPrimitive = dynamic_cast<DataPrimitive*>(dataStructure);
                if (!dataPrimitive)
                {
                    Log::Error("Mismatch with stored type and expected type.");
                    return false;
                }

                dataPrimitive->Write(dataPtr, dataSize);
            }
            else
            {
                auto dataFixed = dynamic_cast<DataFixed*>(dataStructure);
                if (!dataFixed)
                {
                    Log::Error("Mismatch with stored type and expected type.");
                    return false;
                }

                dataFixed->WriteRaw(dataPtr, dataSize);
            }

            dataPtr += dataSize;
            dataRemaining -= dataSize;
        }
    }

    return true;
}

std::byte* Pine::Serialization::Serializer::Write(size_t& outputSize) const
{
    outputSize = sizeof(FileHeader);

    for (const auto& data : m_Data)
    {
        size_t dataSize = data->GetDataSize();

        outputSize += PINE_COMPACT_MODE ? sizeof(DataHeader) : (sizeof(DataHeader) + sizeof(std::uint8_t) + strlen(data->GetName()) * sizeof(char));

        if (static_cast<std::uint8_t>(data->GetType()) >= static_cast<std::uint8_t>(DataType::String))
        {
            // Data size required as its own field.
            outputSize += sizeof(std::uint32_t);

            if (data->GetType() == DataType::Array)
            {
                auto dataArray = dynamic_cast<DataArray*>(data);
                assert(dataArray != nullptr);

                // Another byte required for the array count.
                outputSize += sizeof(std::uint32_t);
            }
        }

        outputSize += dataSize;
    }

    auto data = new std::byte[outputSize];

    auto fileHeader = reinterpret_cast<FileHeader*>(data);

    fileHeader->Magic = PINE_MAGIC;
    fileHeader->Version = PINE_VERSION;
    fileHeader->Flags = 0;
    fileHeader->DataCount = m_Data.size();

    if constexpr (!PINE_COMPACT_MODE)
    {
        fileHeader->Flags |= static_cast<std::uint8_t>(FileHeaderFlags::FlexibleMode);
    }

    char* dataPtr = reinterpret_cast<char*>(data) + sizeof(FileHeader);
    size_t dataRemaining = outputSize - sizeof(FileHeader);

    for (const auto& dataProperty : m_Data)
    {
        if (sizeof(DataHeader) > dataRemaining)
        {
            throw std::runtime_error("Ran out data while writing.");
        }

        auto dataHeader = reinterpret_cast<DataHeader*>(dataPtr);

        dataHeader->Type = static_cast<std::uint8_t>(dataProperty->GetType());

        if (!PINE_COMPACT_MODE)
        {
            const auto headerTotalSize = (sizeof(DataHeader) + sizeof(std::uint8_t) + strlen(dataProperty->GetName()) * sizeof(char));

            if (headerTotalSize > dataRemaining)
            {
                throw std::runtime_error("Ran out data while writing.");
            }

            auto flexibleHeader = reinterpret_cast<DataHeaderFlexible*>(dataPtr);

            if (strlen(dataProperty->GetName()) > 31) // 31 instead of 32 to account for null character.
            {
                Log::Error("Data property name too large while writing.");
                free(data);
                return nullptr;
            }

            flexibleHeader->DataNameLength = strlen(dataProperty->GetName());

            memcpy(&flexibleHeader->DataName, dataProperty->GetName(), flexibleHeader->DataNameLength);

            dataPtr += headerTotalSize;
            dataRemaining -= headerTotalSize;
        }
        else
        {
            dataPtr += sizeof(DataHeader);
            dataRemaining -= sizeof(DataHeader);
        }

        bool dynamicSize = false;

        if (static_cast<std::uint8_t>(dataProperty->GetType()) >= static_cast<std::uint8_t>(DataType::String))
        {
            *reinterpret_cast<std::uint32_t*>(dataPtr) = dataProperty->GetDataSize();

            dataPtr += sizeof(std::uint32_t);
            dataRemaining -= sizeof(std::uint32_t);

            dynamicSize = true;
        }

        if (dataProperty->GetType() == DataType::Array)
        {
            auto dataArray = dynamic_cast<DataArray*>(dataProperty);

            assert(dataArray != nullptr);

            *reinterpret_cast<std::uint32_t*>(dataPtr) = dataArray->GetDataCount();

            dataPtr += sizeof(std::uint32_t);
            dataRemaining -= sizeof(std::uint32_t);
        }

        if (dataProperty->GetDataSize() > dataRemaining)
        {
            throw std::runtime_error("Ran out data while writing.");
        }

        if (dataProperty->GetType() == DataType::Array)
        {
            auto dataArray = dynamic_cast<DataArray*>(dataProperty);

            assert(dataArray != nullptr);

            for (size_t i{};i < dataArray->GetDataCount();i++)
            {
                const auto& arrayData = dataArray->GetData(i);
                const auto arrayDataTotalSize = arrayData.size + sizeof(std::uint32_t);

                if (arrayDataTotalSize > dataRemaining)
                {
                    throw std::runtime_error("Ran out data while writing.");
                }

                *reinterpret_cast<std::uint32_t*>(dataPtr) = arrayData.size;

                memcpy(dataPtr + sizeof(std::uint32_t), arrayData.data, arrayData.size);

                dataPtr += arrayDataTotalSize;
                dataRemaining -= arrayDataTotalSize;
            }
        }
        else
        {
            if (dynamicSize)
            {
                auto dataFixed = dynamic_cast<DataFixed*>(dataProperty);

                assert(dataFixed != nullptr);

                memcpy(dataPtr, dataFixed->GetData(), dataFixed->GetDataSize());
            }
            else
            {
                auto dataPrimitive = dynamic_cast<DataPrimitive*>(dataProperty);
                assert(dataPrimitive != nullptr);

                memcpy(dataPtr, dataPrimitive->GetData(), dataProperty->GetDataSize());
            }

            dataPtr += dataProperty->GetDataSize();
            dataRemaining -= dataProperty->GetDataSize();
        }
    }

    return data;
}
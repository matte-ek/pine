#include "Serialization.hpp"

#include <utility>

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

    enum class DataHeaderFlags : std::uint8_t
    {
        Array = (1 << 0)
    };

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
        std::uint8_t Flags;
    };

    struct DataHeaderFlexible : DataHeader
    {
        // This could (and probably should?) be dynamic to save memory.
        char DataName[32];
    };

    struct DataArray
    {
        std::uint32_t Size;
        std::uint32_t Count;
    };

    struct DataVariableSize
    {
        size_t DataSize;
    };

#pragma pack(pop)
}

Pine::Serialization::Data::Data(
    Serializer* parentSerializer,
    DataType type,
    const char* name,
    size_t size)
    :
    m_Name(name),
    m_Type(type),
    m_DataSize(size)
{
    parentSerializer->m_Data.push_back(this);
}

Pine::Serialization::Data::~Data()
{
    if (m_VariableData)
    {
        free(m_VariableData);
        m_VariableData = nullptr;
    }
}

const char* Pine::Serialization::Data::ReadString() const
{
    return static_cast<const char*>(m_VariableData);
}

void Pine::Serialization::Data::WriteString(const char* str)
{
    m_DataSize = strlen(str) +  1;
    m_VariableData = malloc(m_DataSize);

    memcpy(m_VariableData, str, m_DataSize);
}

void Pine::Serialization::Data::WriteString(const std::string& str)
{
    WriteString(str.c_str());
}

void* Pine::Serialization::Data::GetData() const
{
    return m_VariableData;
}

size_t Pine::Serialization::Data::GetDataSize() const
{
    return m_DataSize;
}

bool Pine::Serialization::Serializer::Read(void* data, size_t size) const
{
    if (!data || size < sizeof(FileHeader))
    {
        return false;
    }

    auto header = static_cast<FileHeader*>(data);

    if (header->Magic != PINE_MAGIC)
    {
        return false;
    }

    if (PINE_COMPACT_MODE)
    {
        // If we're in compact mode make sure this file is encoded
        // with the correct version.
        if (header->Version != PINE_VERSION)
        {
            return false;
        }
    }
    else
    {
        // If we're in flexible, make sure the file is also in flexible mode,
        // if the version is the same we could probably maybe still deal with it but w/e
        if (!(header->Flags & static_cast<std::uint16_t>(FileHeaderFlags::FlexibleMode)))
        {
            return false;
        }
    }

    char* dataPtr = static_cast<char*>(data) + sizeof(FileHeader);
    size_t dataRemaining = size - sizeof(FileHeader);

    for (std::uint32_t i = 0; i < header->DataCount; i++)
    {
        // Make sure we still have data left.
        if (sizeof(DataHeader) > dataRemaining)
        {
            Log::Error("Serialization: Ran out of data while parsing pine file, probably corrupted.");
            return false;
        }

        auto dataHeader = reinterpret_cast<DataHeader*>(dataPtr);

        if (dataHeader->Type == 0 || dataHeader->Type >= static_cast<std::uint8_t>(DataType::Count))
        {
            Log::Error("Serialization: Invalid data type while parsing file, probably corrupted.");
            return false;
        }

        char dataName[32];

        if (!PINE_COMPACT_MODE)
        {
            if (sizeof(DataHeaderFlexible) > dataRemaining)
            {
                Log::Error("Serialization: Ran out of data while parsing pine file, probably corrupted.");
                return false;
            }

            auto flexibleHeader = reinterpret_cast<DataHeaderFlexible*>(dataPtr);

            memcpy(dataName, flexibleHeader->DataName, 32);

            dataPtr += sizeof(DataHeaderFlexible);
            dataRemaining -= sizeof(DataHeaderFlexible);
        }
        else
        {
            if (i >= m_Data.size())
            {
                Log::Error("Serialization: Invalid data index while reading data.");
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
            switch (static_cast<DataType>(dataHeader->Type))
            {
                case DataType::Int32:
                    dataSize = sizeof(std::int32_t);
                    break;
                case DataType::Float32:
                    dataSize = sizeof(float);
                    break;
                case DataType::Vec2:
                    dataSize = sizeof(Vector2f);
                    break;
                case DataType::Vec3:
                    dataSize = sizeof(Vector3f);
                    break;
                case DataType::Vec4:
                    dataSize = sizeof(Vector4f);
                    break;
                case DataType::Quaternion:
                    dataSize = sizeof(Quaternion);
                    break;
                default:
                    break;
            }
        }
        else
        {
            if (dataRemaining < sizeof(std::uint32_t))
            {
                Log::Error("Serialization: Ran out of data while reading data?");
                return false;
            }

            dataSize = *reinterpret_cast<std::uint32_t*>(dataPtr);

            dataPtr += sizeof(std::uint32_t);
            dataRemaining -= sizeof(std::uint32_t);
            dynamicSize = true;

            // TODO: Remove me, used while testing.
            if (dataSize >= 0x1000000)
            {
                Log::Error("Serialization: CHECK ME: Size big while reading data");
                return false;
            }
        }

        if (dataSize == 0)
        {
            Log::Error("Serialization: Invalid data type while reading data?");
            return false;
        }

        if (dataSize > dataRemaining)
        {
            Log::Error("Serialization: Ran out of data while reading data?");
            return false;
        }

        Data* dataStructure = nullptr;

        if (PINE_COMPACT_MODE)
        {
            dataStructure = m_Data[i];
        }
        else
        {
            for (auto dataCandidate : m_Data)
            {
                if (strcmp(dataCandidate->m_Name, dataName) == 0)
                {
                    dataStructure = dataCandidate;
                    break;
                }
            }
        }

        if (!dataStructure)
        {
            Log::Error("Serialization: Could not find data structure while reading data.");
            return false;
        }

        if (static_cast<std::uint8_t>(dataStructure->m_Type) != dataHeader->Type)
        {
            Log::Error("Serialization: Mismatch data types while reading data.");
            return false;
        }

        dataStructure->m_DataSize = dataSize;

        if (dynamicSize)
        {
            dataStructure->m_VariableData = malloc(dataSize);
            memcpy(dataStructure->m_VariableData, dataPtr, dataSize);
        }
        else
        {
            memcpy(dataStructure->m_Data, dataPtr, dataSize);
        }

        dataPtr += dataSize;
        dataRemaining -= dataSize;
    }

    return true;
}

void* Pine::Serialization::Serializer::Write(size_t& outputSize) const
{
    outputSize = sizeof(FileHeader);

    for (const auto& data : m_Data)
    {
        if (data->m_DataSize == 0)
        {
            Log::Error("Serialization: Invalid data size while writing data.");
            return nullptr;
        }

        outputSize += PINE_COMPACT_MODE ? sizeof(DataHeader) : sizeof(DataHeaderFlexible);

        if (data->m_DataSize > 16)
        {
            // Data size required as its own field.
            outputSize += sizeof(std::uint32_t);
        }

        outputSize += data->m_DataSize;
    }

    void* data = malloc(outputSize);

    auto fileHeader = static_cast<FileHeader*>(data);

    fileHeader->Magic = PINE_MAGIC;
    fileHeader->Version = PINE_VERSION;
    fileHeader->Flags = 0;
    fileHeader->DataCount = m_Data.size();

    if (!PINE_COMPACT_MODE)
    {
        fileHeader->Flags |= static_cast<std::uint8_t>(FileHeaderFlags::FlexibleMode);
    }

    char* dataPtr = static_cast<char*>(data) + sizeof(FileHeader);
    size_t dataRemaining = outputSize - sizeof(FileHeader);

    for (const auto& dataProperty : m_Data)
    {
        if (sizeof(DataHeader) > dataRemaining)
        {
            Log::Error("Serialization: Ran out of data while writing pine file?");
            free(data);
            return nullptr;
        }

        auto dataHeader = reinterpret_cast<DataHeader*>(dataPtr);

        dataHeader->Type = static_cast<std::uint8_t>(dataProperty->m_Type);

        if (!PINE_COMPACT_MODE)
        {
            if (sizeof(DataHeaderFlexible) > dataRemaining)
            {
                Log::Error("Serialization: Ran out of data while writing pine file?");
                free(data);
                return nullptr;
            }

            auto flexibleHeader = reinterpret_cast<DataHeaderFlexible*>(dataPtr);

            if (strlen(dataProperty->m_Name) > 31) // 31 instead of 32 to account for null character.
            {
                Log::Error("Serialization: Data property name too large while writing.");
                free(data);
                return nullptr;
            }

            strcpy_s(flexibleHeader->DataName, dataProperty->m_Name);

            dataPtr += sizeof(DataHeaderFlexible);
            dataRemaining -= sizeof(DataHeaderFlexible);
        }
        else
        {
            dataPtr += sizeof(DataHeader);
            dataRemaining -= sizeof(DataHeader);
        }

        bool dynamicSize = false;

        if (static_cast<std::uint8_t>(dataProperty->m_Type) >= static_cast<std::uint8_t>(DataType::String))
        {
            *reinterpret_cast<std::uint32_t*>(dataPtr) = dataProperty->m_DataSize;

            dataPtr += sizeof(std::uint32_t);
            dataRemaining -= sizeof(std::uint32_t);

            dynamicSize = true;
        }

        if (dataProperty->m_DataSize > dataRemaining)
        {
            Log::Error("Serialization: Ran out of data while writing pine file?");
            free(data);
            return nullptr;
        }

        if (dynamicSize)
        {
            memcpy(dataPtr, dataProperty->m_VariableData, dataProperty->m_DataSize);
        }
        else
        {
            memcpy(dataPtr, dataProperty->m_Data, dataProperty->m_DataSize);
        }

        dataPtr += dataProperty->m_DataSize;
        dataRemaining -= dataProperty->m_DataSize;
    }

    return data;
}
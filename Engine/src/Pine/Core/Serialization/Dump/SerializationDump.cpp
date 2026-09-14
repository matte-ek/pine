#include "SerializationDump.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "Pine/Core/Serialization/Serialization.hpp"

namespace
{
    using namespace Pine::Serialization;

    // Corrupt or misidentified data could otherwise nest until the stack runs out. Real content is
    // nowhere near this deep: asset -> payload -> entity -> component -> fields is about five.
    constexpr int m_MaxDepth = 32;

    // A bounds-checked cursor over the buffer, so a malformed blob fails cleanly rather than walking
    // off the end of it.
    class Cursor
    {
    private:
        const std::byte* m_Data;
        std::size_t m_Size;
        std::size_t m_Offset = 0;
    public:
        Cursor(const std::byte* data, const std::size_t size)
            : m_Data(data), m_Size(size)
        {
        }

        std::size_t GetOffset() const
        {
            return m_Offset;
        }

        bool Take(const std::size_t count, const std::byte*& bytes)
        {
            if (count > m_Size - m_Offset)
            {
                return false;
            }

            bytes = m_Data + m_Offset;
            m_Offset += count;

            return true;
        }

        template <typename TValue>
        bool Take(TValue& value)
        {
            const std::byte* bytes = nullptr;

            if (!Take(sizeof(TValue), bytes))
            {
                return false;
            }

            // Copied out rather than cast in place: nothing guarantees the buffer is aligned for the
            // type, and the headers are packed.
            std::memcpy(&value, bytes, sizeof(TValue));

            return true;
        }
    };

    // Length of the valid UTF-8 sequence starting at bytes[0], or 0 if it is not one. Strict on
    // purpose - overlong encodings and surrogate halves are rejected, because nlohmann rejects them
    // too and the whole point is that nothing downstream can throw.
    int Utf8SequenceLength(const unsigned char* bytes, const std::size_t available)
    {
        const auto isContinuation = [](const unsigned char byte)
        {
            return (byte & 0xC0) == 0x80;
        };

        const unsigned char lead = bytes[0];

        if (lead < 0x80)
        {
            return 1;
        }

        if (lead >= 0xC2 && lead <= 0xDF)
        {
            return available >= 2 && isContinuation(bytes[1]) ? 2 : 0;
        }

        if (lead >= 0xE0 && lead <= 0xEF)
        {
            if (available < 3 || !isContinuation(bytes[1]) || !isContinuation(bytes[2]))
            {
                return 0;
            }

            if (lead == 0xE0 && bytes[1] < 0xA0) return 0;  // overlong
            if (lead == 0xED && bytes[1] >= 0xA0) return 0; // UTF-16 surrogate half

            return 3;
        }

        if (lead >= 0xF0 && lead <= 0xF4)
        {
            if (available < 4 || !isContinuation(bytes[1]) || !isContinuation(bytes[2]) || !isContinuation(bytes[3]))
            {
                return 0;
            }

            if (lead == 0xF0 && bytes[1] < 0x90) return 0;  // overlong
            if (lead == 0xF4 && bytes[1] >= 0x90) return 0; // beyond U+10FFFF

            return 4;
        }

        return 0;
    }

    // Field names and String payloads are raw bytes out of the file - nothing guarantees they are
    // text at all, and a corrupt or misidentified buffer readily produces bytes that are not valid
    // UTF-8. Escaping them keeps the value readable and, more importantly, keeps serializing the
    // result from becoming a second failure.
    std::string SanitizeUtf8(const char* bytes, const std::size_t size)
    {
        const auto unsignedBytes = reinterpret_cast<const unsigned char*>(bytes);

        std::string sanitized;
        sanitized.reserve(size);

        std::size_t index = 0;

        while (index < size)
        {
            const int sequenceLength = Utf8SequenceLength(unsignedBytes + index, size - index);

            if (sequenceLength == 0)
            {
                char escape[5];

                std::snprintf(escape, sizeof(escape), "\\x%02X", unsignedBytes[index]);

                sanitized += escape;
                index++;

                continue;
            }

            sanitized.append(bytes + index, sequenceLength);
            index += static_cast<std::size_t>(sequenceLength);
        }

        return sanitized;
    }

    template <typename TValue>
    TValue ReadValue(const std::byte* bytes)
    {
        TValue value{};

        std::memcpy(&value, bytes, sizeof(TValue));

        return value;
    }

    nlohmann::json StoreVector(const float* components, const int count)
    {
        static const char* names[] = { "x", "y", "z", "w" };

        nlohmann::json json;

        for (int component = 0; component < count; component++)
        {
            json[names[component]] = components[component];
        }

        return json;
    }

    nlohmann::json StorePrimitive(const DataType type, const std::byte* bytes)
    {
        switch (type)
        {
        case DataType::Boolean:
            return ReadValue<bool>(bytes);
        case DataType::Int32:
            return ReadValue<std::int32_t>(bytes);
        case DataType::Int64:
            return ReadValue<std::int64_t>(bytes);
        case DataType::Float32:
            return ReadValue<float>(bytes);
        case DataType::Vec2:
        {
            const auto value = ReadValue<Pine::Vector2f>(bytes);
            return StoreVector(&value.x, 2);
        }
        case DataType::Vec3:
        {
            const auto value = ReadValue<Pine::Vector3f>(bytes);
            return StoreVector(&value.x, 3);
        }
        case DataType::Vec4:
        {
            const auto value = ReadValue<Pine::Vector4f>(bytes);
            return StoreVector(&value.x, 4);
        }
        case DataType::Quaternion:
        {
            const auto value = ReadValue<Pine::Quaternion>(bytes);
            return StoreVector(&value.x, 4);
        }
        case DataType::UId:
            return ReadValue<Pine::UId>(bytes).ToString();
        default:
            return nullptr;
        }
    }

    bool ParseBlob(const std::byte* data, std::size_t size, nlohmann::json& fields, std::size_t& consumed, int depth);

    // A Data field or an array element is usually another serializer blob written whole - that is
    // what makes a level come back as a tree. Anything that does not parse cleanly *and* account for
    // its whole buffer is reported as opaque rather than guessed at.
    nlohmann::json DescribePayload(const std::byte* data, const std::size_t size, const int depth)
    {
        if (depth < m_MaxDepth)
        {
            nlohmann::json nested;
            std::size_t nestedConsumed = 0;

            if (ParseBlob(data, size, nested, nestedConsumed, depth + 1) && nestedConsumed == size)
            {
                return nested;
            }
        }

        nlohmann::json opaque;

        opaque["__type"] = "data";
        opaque["__size"] = size;

        return opaque;
    }

    bool ParseBlob(const std::byte* data, const std::size_t size, nlohmann::json& fields, std::size_t& consumed, const int depth)
    {
        Cursor cursor(data, size);

        Internal::FileHeader header{};

        if (!cursor.Take(header))
        {
            return false;
        }

        // These three together are what make the nested-blob sniff safe: a false positive would have
        // to match the magic, the version and the mode flag, and then parse to exactly its length.
        if (header.Magic != Internal::Magic || header.Version != Internal::Version)
        {
            return false;
        }

        if (!(header.Flags & static_cast<std::uint8_t>(Internal::FileHeaderFlags::FlexibleMode)))
        {
            return false;
        }

        fields = nlohmann::json::object();

        for (std::uint16_t field = 0; field < header.DataCount; field++)
        {
            Internal::DataHeaderFlexible fieldHeader{};

            if (!cursor.Take(fieldHeader))
            {
                return false;
            }

            if (fieldHeader.Type == 0 || fieldHeader.Type >= static_cast<std::uint8_t>(DataType::Count))
            {
                return false;
            }

            const std::byte* nameBytes = nullptr;

            if (!cursor.Take(fieldHeader.DataNameLength, nameBytes))
            {
                return false;
            }

            const auto name = SanitizeUtf8(reinterpret_cast<const char*>(nameBytes), fieldHeader.DataNameLength);
            const auto type = static_cast<DataType>(fieldHeader.Type);

            // Everything ordered before String has a size the type itself determines.
            if (type < DataType::String)
            {
                const std::byte* valueBytes = nullptr;

                if (!cursor.Take(Internal::PrimitiveDataTypeToSize(type), valueBytes))
                {
                    return false;
                }

                fields[name] = StorePrimitive(type, valueBytes);

                continue;
            }

            std::uint32_t payloadSize = 0;

            if (!cursor.Take(payloadSize))
            {
                return false;
            }

            if (type == DataType::Array)
            {
                // Arrays carry their element count separately, and each element is length-prefixed,
                // so the walk goes by count rather than by payloadSize.
                std::uint32_t elementCount = 0;

                if (!cursor.Take(elementCount))
                {
                    return false;
                }

                auto elements = nlohmann::json::array();

                for (std::uint32_t element = 0; element < elementCount; element++)
                {
                    std::uint32_t elementSize = 0;

                    if (!cursor.Take(elementSize))
                    {
                        return false;
                    }

                    const std::byte* elementBytes = nullptr;

                    if (!cursor.Take(elementSize, elementBytes))
                    {
                        return false;
                    }

                    elements.push_back(DescribePayload(elementBytes, elementSize, depth));
                }

                fields[name] = elements;

                continue;
            }

            const std::byte* payloadBytes = nullptr;

            if (!cursor.Take(payloadSize, payloadBytes))
            {
                return false;
            }

            if (type == DataType::String)
            {
                fields[name] = SanitizeUtf8(reinterpret_cast<const char*>(payloadBytes), payloadSize);

                continue;
            }

            fields[name] = DescribePayload(payloadBytes, payloadSize, depth);
        }

        consumed = cursor.GetOffset();

        return true;
    }
}

std::optional<nlohmann::json> Pine::Serialization::Dump::ToJson(const void* data, const std::size_t size)
{
    if (data == nullptr || size == 0)
    {
        return std::nullopt;
    }

    nlohmann::json fields;
    std::size_t consumed = 0;

    if (!ParseBlob(static_cast<const std::byte*>(data), size, fields, consumed, 0))
    {
        return std::nullopt;
    }

    // Serializer::Read() only warns about trailing bytes rather than rejecting them, so they are
    // tolerated here too - but reported, since they usually mean the buffer was not what the caller
    // thought it was.
    if (consumed != size)
    {
        fields["__trailingBytes"] = size - consumed;
    }

    return fields;
}

std::optional<nlohmann::json> Pine::Serialization::Dump::ToJson(const ByteSpan& span)
{
    return ToJson(span.data, span.size);
}

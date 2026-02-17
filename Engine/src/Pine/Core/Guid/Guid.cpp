#include "Guid.hpp"

#include <cassert>

#include "Pine/Core/Span/Span.hpp"

#ifdef _WIN32
#include "rpc.h"
#endif

Pine::Guid::Guid(const GuidData& data)
    : m_GuidData(data)
{
}

Pine::Guid::Guid()
    : m_GuidData{}
{
}

Pine::Guid::Guid(const ByteSpan& data)
    : m_GuidData()
{
    assert(sizeof(GuidData) > data.size);
    memcpy(&m_GuidData, data.data, sizeof(GuidData));
}

std::string Pine::Guid::ToString() const
{
    char buffer[37];

    std::snprintf(
        buffer, sizeof(buffer),
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        m_GuidData.Data1,
        m_GuidData.Data2,
        m_GuidData.Data3,
        m_GuidData.Data4[0], m_GuidData.Data4[1],
        m_GuidData.Data4[2], m_GuidData.Data4[3], m_GuidData.Data4[4],
        m_GuidData.Data4[5], m_GuidData.Data4[6], m_GuidData.Data4[7]
    );

    return buffer;
}

Pine::Guid Pine::Guid::New()
{
    // This is crap, fix fix fix.
#ifdef _WIN32
    UUID uuid;
    UuidCreate(&uuid);
    return Guid(GuidData{uuid.Data1, uuid.Data2, uuid.Data3, uuid.Data4[0]});
#else
    #error TODO: Platform support
#endif
}

bool Pine::Guid::operator==(const Guid& other) const
{
    auto ptr = reinterpret_cast<const std::uint64_t*>(&m_GuidData);
    auto ptrOther = reinterpret_cast<const std::uint64_t*>(&other.m_GuidData);

    return ptr[0] == ptrOther[0] && ptr[1] == ptrOther[1];
}

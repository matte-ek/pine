#include "UId.hpp"

#include <cassert>
#include <chrono>
#include <random>

#include "Pine/Core/Span/Span.hpp"

Pine::UId::UId(const ByteSpan& data)
{
    assert(sizeof(UId) == data.size);

    m_Time = *reinterpret_cast<const std::uint64_t*>(data.data);
    m_Random = *reinterpret_cast<const std::uint64_t*>(data.data + 8);
}

Pine::UId::UId(std::string_view str)
{
    sscanf_s(str.data(), "%llx-%016llx", &m_Time, &m_Random);
}

std::uint64_t Pine::UId::GetTime() const
{
    return m_Time;
}

std::uint64_t Pine::UId::GetRandom() const
{
    return m_Random;
}

std::string Pine::UId::ToString() const
{
    char buffer[34];

    std::snprintf(
        buffer, sizeof(buffer),
        "%llx-%016llx",
        m_Time,
        m_Random
    );

    return buffer;
}

bool Pine::UId::operator==(const UId& other) const
{
    return this->m_Random == other.m_Random && this->m_Time == other.m_Time;
}

bool Pine::UId::operator!=(const UId& other) const
{
    return !(*this == other);
}

Pine::UId Pine::UId::New()
{
    static std::mt19937_64 rng(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dist;

    UId uid{};

    uid.m_Time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    uid.m_Random = dist(rng);

    return uid;
}

Pine::UId Pine::UId::Empty()
{
    return UId();
}

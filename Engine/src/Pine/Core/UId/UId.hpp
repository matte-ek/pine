#pragma once
#include <functional>
#include <string>

namespace Pine
{
    class ByteSpan;

    // This is not as random as a traditional GUID/UUID, but will give us
    // around 2^64 possibilities for every single nanosecond, which should be fine
    // for this game engine, I'd hope.
    class UId
    {
    private:
        std::uint64_t m_Time{};
        std::uint64_t m_Random{};
    public:
        UId() = default;

        explicit UId(const ByteSpan& data);
        explicit UId(std::string_view str);

        std::uint64_t GetTime() const;
        std::uint64_t GetRandom() const;

        std::string ToString() const;

        bool operator==(const UId& other) const;
        bool operator!=(const UId& other) const;

        static UId New();
        static UId Empty();
    };
}

template<>
struct std::hash<Pine::UId>
{
    std::size_t operator()(const Pine::UId& guid) const noexcept
    {
        return std::hash<std::uint64_t>{}(guid.GetTime()) ^ std::hash<std::uint64_t>{}(guid.GetRandom());
    }
};
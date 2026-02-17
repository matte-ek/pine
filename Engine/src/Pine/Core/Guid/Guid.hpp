#pragma once
#include <functional>
#include <string>

namespace Pine
{
    class ByteSpan;

    class Guid
    {
    private:
        struct GuidData
        {
            std::uint32_t Data1;
            std::uint16_t Data2;
            std::uint16_t Data3;
            std::uint8_t Data4[8];
        };

        GuidData m_GuidData{};

        explicit Guid(const GuidData& data);
    public:
        Guid();

        explicit Guid(const ByteSpan& data);

        std::string ToString() const;

        bool operator==(const Guid& other) const;

        static Guid New();
    };
}

template<>
struct std::hash<Pine::Guid>
{
    std::size_t operator()(const Pine::Guid& guid) const noexcept
    {
        // This is really hacky, but w/e.
        auto ptr = reinterpret_cast<const std::uint64_t*>(&guid);
        return std::hash<std::uint64_t>{}(ptr[0]) ^ std::hash<std::uint64_t>{}(ptr[1]);
    }
};
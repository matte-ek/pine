#include "ScriptFieldRegistry.hpp"

#include "Pine/Core/Log/Log.hpp"
#include "../ManagedCall/ManagedCall.hpp"

namespace
{
    constexpr auto FieldRegistryTypeName = "Pine.Core.Reflection.FieldRegistry, Pine";

    // The managed registry's entry points, resolved once. All of them are static, and all of them
    // answer with a failure value rather than throwing - see the class' own comment.
    struct EntryPoints
    {
        std::int32_t (*Register)(std::uint64_t classTypeHandle) = nullptr;
        std::int32_t (*GetFieldCount)(std::int32_t classId) = nullptr;
        std::int32_t (*ReadDescriptors)(std::int32_t classId, void* buffer, std::int32_t capacity) = nullptr;
        std::int32_t (*GetDescriptorSize)() = nullptr;
        std::int32_t (*MeasureValue)(std::uint64_t object, std::int32_t classId, std::int32_t fieldIndex) = nullptr;
        std::int32_t (*ReadValue)(std::uint64_t object, std::int32_t classId, std::int32_t fieldIndex,
            void* buffer, std::int32_t capacity) = nullptr;
        std::int32_t (*WriteValue)(std::uint64_t object, std::int32_t classId, std::int32_t fieldIndex,
            const void* buffer, std::int32_t length) = nullptr;
        void (*Reset)() = nullptr;
    };

    EntryPoints m_EntryPoints;
}

bool Pine::Script::FieldRegistry::Setup()
{
    m_EntryPoints = {};

    m_EntryPoints.Register = ManagedCall::Find<decltype(EntryPoints::Register)>(
        FieldRegistryTypeName, "Register");
    m_EntryPoints.GetFieldCount = ManagedCall::Find<decltype(EntryPoints::GetFieldCount)>(
        FieldRegistryTypeName, "GetFieldCount");
    m_EntryPoints.ReadDescriptors = ManagedCall::Find<decltype(EntryPoints::ReadDescriptors)>(
        FieldRegistryTypeName, "ReadDescriptors");
    m_EntryPoints.GetDescriptorSize = ManagedCall::Find<decltype(EntryPoints::GetDescriptorSize)>(
        FieldRegistryTypeName, "GetDescriptorSize");
    m_EntryPoints.MeasureValue = ManagedCall::Find<decltype(EntryPoints::MeasureValue)>(
        FieldRegistryTypeName, "MeasureValue");
    m_EntryPoints.ReadValue = ManagedCall::Find<decltype(EntryPoints::ReadValue)>(
        FieldRegistryTypeName, "ReadValue");
    m_EntryPoints.WriteValue = ManagedCall::Find<decltype(EntryPoints::WriteValue)>(
        FieldRegistryTypeName, "WriteValue");
    m_EntryPoints.Reset = ManagedCall::Find<decltype(EntryPoints::Reset)>(
        FieldRegistryTypeName, "Reset");

    if (m_EntryPoints.GetDescriptorSize == nullptr)
    {
        return false;
    }

    // Descriptors cross as an array of raw structs, so the two definitions have to agree on the
    // stride. Comparing the sizes catches a member added to one side and not the other, which is
    // the way that would otherwise go wrong quietly.
    const auto managedSize = m_EntryPoints.GetDescriptorSize();

    if (managedSize != static_cast<std::int32_t>(sizeof(ScriptFieldDescriptor)))
    {
        PError(fmt::format("Script field descriptor is {} bytes in Pine.dll and {} in the engine; "
                           "script fields will not be reflected.",
            managedSize, sizeof(ScriptFieldDescriptor)));

        m_EntryPoints = {};

        return false;
    }

    return true;
}

void Pine::Script::FieldRegistry::Reset()
{
    if (m_EntryPoints.Reset == nullptr)
    {
        return;
    }

    m_EntryPoints.Reset();
}

int Pine::Script::FieldRegistry::Register(const std::uint64_t classTypeHandle)
{
    if (m_EntryPoints.Register == nullptr || classTypeHandle == 0)
    {
        return -1;
    }

    return m_EntryPoints.Register(classTypeHandle);
}

std::vector<Pine::Script::ScriptFieldDescriptor> Pine::Script::FieldRegistry::GetDescriptors(const int classId)
{
    if (m_EntryPoints.GetFieldCount == nullptr)
    {
        return {};
    }

    const auto count = m_EntryPoints.GetFieldCount(classId);

    if (count <= 0)
    {
        return {};
    }

    std::vector<ScriptFieldDescriptor> descriptors(count);

    if (m_EntryPoints.ReadDescriptors(classId, descriptors.data(), count) != count)
    {
        return {};
    }

    return descriptors;
}

bool Pine::Script::FieldRegistry::ReadValue(const ObjectHandle& object, const int classId, const int fieldIndex,
    std::vector<std::byte>& data)
{
    if (m_EntryPoints.MeasureValue == nullptr)
    {
        return false;
    }

    const auto length = m_EntryPoints.MeasureValue(object.Id, classId, fieldIndex);

    if (length < 0)
    {
        return false;
    }

    data.assign(length, std::byte{});

    // Nothing to fetch for an empty string or a reference to nothing, and no buffer to hand over.
    if (length == 0)
    {
        return true;
    }

    if (m_EntryPoints.ReadValue(object.Id, classId, fieldIndex, data.data(), length) != length)
    {
        data.clear();

        return false;
    }

    return true;
}

bool Pine::Script::FieldRegistry::WriteValue(const ObjectHandle& object, const int classId, const int fieldIndex,
    const std::vector<std::byte>& data)
{
    if (m_EntryPoints.WriteValue == nullptr)
    {
        return false;
    }

    // An empty value is meaningful, and managed code never reads the buffer in that case, but it
    // still has to be handed a pointer it can hold.
    const void* buffer = data.empty() ? static_cast<const void*>("") : data.data();

    return m_EntryPoints.WriteValue(object.Id, classId, fieldIndex, buffer,
        static_cast<std::int32_t>(data.size())) == 1;
}

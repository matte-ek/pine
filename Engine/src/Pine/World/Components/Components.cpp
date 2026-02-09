#include "Components.hpp"

#include "AudioListener/AudioListener.hpp"
#include "AudioSource/AudioSource.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/World/Components/Collider2D/Collider2D.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/NativeScript/NativeScript.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/RigidBody2D/RigidBody2D.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "TerrainRenderer/TerrainRendererComponent.hpp"

using namespace Pine;

namespace
{
    std::vector<ComponentDataBlock<Component>*> m_ComponentDataBlocks;

    bool ResizeComponentDataBlock(ComponentDataBlock<Component>* block, std::uint32_t size)
    {
        // Allocate the new data
        void* arrayData = malloc(block->m_ComponentSize * size);
        const auto arrayOccupationData = new bool[size];

        if (!arrayData)
        {
            Log::Error("Failure allocating data for component block");
            return false;
        }

        memset(arrayData, 0, block->m_ComponentSize * size);
        memset(arrayOccupationData, 0, sizeof(bool) * size);

        void* oldArrayData = nullptr;
        void* oldOccupationArray = nullptr;

        // Copy over the old data if we have any
        if (block->m_ComponentArray != nullptr &&
            block->m_ComponentOccupationArray != nullptr)
        {
            std::size_t arrayBlockCopySize = block->m_ComponentArraySize;
            std::size_t occupationArrayCopySize = block->m_ComponentOccupationArraySize;

            if (block->m_ComponentArraySize > block->m_ComponentSize * size)
            {
                Log::Warning("Smaller new buffer for component data block, data loss possible.");

                arrayBlockCopySize = block->m_ComponentSize * size;
                occupationArrayCopySize = sizeof(bool) * size;
            }

            memcpy(arrayData, block->m_ComponentArray, arrayBlockCopySize);
            memcpy(arrayOccupationData, block->m_ComponentOccupationArray, occupationArrayCopySize);

            oldArrayData = static_cast<void*>(block->m_ComponentArray);
            oldOccupationArray = static_cast<void*>(block->m_ComponentOccupationArray);
        }

        // Set the new array pointers
        block->m_ComponentArray = static_cast<Component*>(arrayData);
        block->m_ComponentArraySize = block->m_ComponentSize * size;

        block->m_ComponentOccupationArray = arrayOccupationData;
        block->m_ComponentOccupationArraySize = sizeof(bool) * size;

        // Set the new size
        block->m_ComponentArrayAllocatedCount = size;

        // Free the old data (if any)
        free(oldArrayData);
        free(oldOccupationArray);

        return true;
    }

    template<typename T>
    ComponentDataBlock<T>* CreateComponentDataBlock(std::uint32_t overrideInstanceCount = 0)
    {
        auto block = new ComponentDataBlock<T>();

        block->m_Component = new T();
        block->m_ComponentSize = sizeof(T);

        std::uint32_t instanceCount = Engine::GetEngineConfiguration().m_MaxObjectCount;

        if (overrideInstanceCount != 0)
            instanceCount = overrideInstanceCount;

        ResizeComponentDataBlock(reinterpret_cast<ComponentDataBlock<Component>*>(block), instanceCount);

        m_ComponentDataBlocks.push_back(reinterpret_cast<ComponentDataBlock<Component>*>(block));

        return block;
    }

    template <class T> std::vector<T>& GetComponentList(ComponentType type)
    {
        // Not going to bother with sanity checking the type, as it's an enum with an already known size.
        // Hopefully, all components specified in the enum are also created in this vector though.
        return *m_ComponentDataBlocks[static_cast<int>(type)]->m_ComponentArray;
    }

    bool m_IgnoreSetHighestEntityIndexFlag = false;
}

void Components::Setup()
{
    CreateComponentDataBlock<Transform>();
    CreateComponentDataBlock<ModelRenderer>();
    CreateComponentDataBlock<TerrainRendererComponent>(32);
    CreateComponentDataBlock<Camera>(32);
    CreateComponentDataBlock<Light>();
    CreateComponentDataBlock<Collider>();
    CreateComponentDataBlock<RigidBody>();
    CreateComponentDataBlock<Collider2D>();
    CreateComponentDataBlock<RigidBody2D>();
    CreateComponentDataBlock<SpriteRenderer>();
    CreateComponentDataBlock<TilemapRenderer>();
    CreateComponentDataBlock<NativeScript>(1); // "Stub" for NativeScript, we cannot create NativeScripts through here, but we need to align the array.
    CreateComponentDataBlock<ScriptComponent>();
    CreateComponentDataBlock<AudioSource>();
    CreateComponentDataBlock<AudioListener>();

    std::size_t totalSize = 0;

    for (const auto& block : m_ComponentDataBlocks)
    {
        totalSize += block->m_ComponentArraySize;
    }

    Log::Verbose("[Components] Total size allocated: " + std::to_string(totalSize / 1024) + " kB (" + std::to_string(Engine::GetEngineConfiguration().m_MaxObjectCount) + " objects per type)");
}

void Components::Shutdown()
{
    for (const auto block : m_ComponentDataBlocks)
    {
        free(block->m_ComponentArray);
        delete[] block->m_ComponentOccupationArray;
    }

    m_ComponentDataBlocks.clear();
}

const std::vector<ComponentDataBlock<Component>*>&Components::GetComponentTypes()
{
    return m_ComponentDataBlocks;
}

Component* Components::Create(ComponentType type, bool standalone)
{
    const auto componentDataBlock = m_ComponentDataBlocks[static_cast<int>(type)];

    Component* component;
    std::uint32_t componentLookupId = 0;
    std::uint64_t uniqueId = 0;

    // Get a pointer to some free memory for the new component, depending on if we want a standalone
    // or in the data block
    if (standalone)
    {
        component = static_cast<Component*>(malloc(componentDataBlock->m_ComponentSize));
    }
    else
    {
        // Find a slot in the array we can use
        const auto newTargetSlot = componentDataBlock->GetAvailableIndex();

        if (newTargetSlot >= componentDataBlock->m_ComponentArrayAllocatedCount)
        {
            // I wouldn't resize the component array right now...
            throw std::runtime_error("Maximum component count reached");

            // We've run out of space in the array, we need to resize it and make it bigger.
            // Not sure if just putting it + 128 is a good idea, but it's fine for now.
            ResizeComponentDataBlock(componentDataBlock, componentDataBlock->m_ComponentArrayAllocatedCount + 128);
        }

        component = componentDataBlock->GetComponent(newTargetSlot);

        componentLookupId = newTargetSlot;
        uniqueId = componentDataBlock->m_UniqueIdCount++;

        // Mark the index as occupied
        componentDataBlock->m_ComponentOccupationArray[newTargetSlot] = true;

        // Store the new highest index
        componentDataBlock->m_HighestComponentIndex = componentDataBlock->GetHighestComponentIndex();
    }

    if (component == nullptr)
    {
        throw std::runtime_error("Component allocation failure.");
    }

    // Copy the data from the 'default' component object
    memcpy(component, componentDataBlock->m_Component, componentDataBlock->m_ComponentSize);

    component->SetStandalone(standalone);
    component->SetInternalId(componentLookupId);
    component->SetUniqueId(uniqueId);

    return component;
}

Component* Components::Copy(Component* component, bool standalone)
{
    const auto newComponent = Create(component->GetType(), standalone);

    nlohmann::json buffer;

    component->SaveData(buffer);
    newComponent->LoadData(buffer);

    newComponent->OnCopied();

    return newComponent;
}

bool Components::Destroy(Component* targetComponent)
{
    if (m_ComponentDataBlocks.empty())
    {
        // If the application is being closed, all the de-constructors in Entity will get called,
        // therefore this method will get called, hence this check.
        return true;
    }

    targetComponent->OnDestroyed();

    // Extra fail-safe to make sure the script object handle is removed, to avoid memory leaks
    // within the scripting engine.
    assert(targetComponent->GetComponentScriptHandle()->Object == nullptr);

    // If the component is standalone (i.e. it isn't in the "ECS"), we can just free
    // the memory and move on.
    if (targetComponent->GetStandalone())
    {
        free(targetComponent);

        return true;
    }

    auto& data = GetData(targetComponent->GetType());
    const auto internalId = targetComponent->GetInternalId();

    // We don't have to free any memory or anything, so marking the slot as "available"
    // should be sufficient.
    data.m_ComponentOccupationArray[internalId] = false;

    // Store the new highest index
    if (!m_IgnoreSetHighestEntityIndexFlag)
        data.m_HighestComponentIndex = data.GetHighestComponentIndex();

    return true;
}

ComponentDataBlock<Component>& Components::GetData(ComponentType type)
{
    return *m_ComponentDataBlocks[static_cast<int>(type)];
}

Component* Components::GetByInternalId(ComponentType type, std::uint32_t internalId)
{
    auto& block = GetData(type);

    return block.GetComponent(internalId);
}

void Components::SetIgnoreHighestEntityIndexFlag(bool ignore)
{
    m_IgnoreSetHighestEntityIndexFlag = ignore;
}

void Components::RecomputeHighestComponentIndex()
{
    for (const auto block : m_ComponentDataBlocks)
    {
        block->m_HighestComponentIndex = block->GetHighestComponentIndex();
    }
}

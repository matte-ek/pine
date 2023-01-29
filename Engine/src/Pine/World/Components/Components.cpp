#include "Components.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"

using namespace Pine;

namespace
{
    std::vector<ComponentDataBlock<IComponent>*> m_ComponentDataBlocks;

    bool ResizeComponentDataBlock(ComponentDataBlock<IComponent>* block, std::uint32_t size)
    {
        // Allocate the new data
        void* arrayData = malloc(block->m_ComponentSize * size);
        bool* arrayOccupationData = new bool[size];

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

            memcpy(arrayData, (void*)block->m_ComponentArray, arrayBlockCopySize);
            memcpy(arrayOccupationData, (void*)block->m_ComponentOccupationArray, occupationArrayCopySize);

            oldArrayData = (void*)block->m_ComponentArray;
            oldOccupationArray = (void*)block->m_ComponentOccupationArray;
        }

        // Set the new array pointers
        block->m_ComponentArray = reinterpret_cast<IComponent*>(arrayData);
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
    ComponentDataBlock<T>* CreateComponentDataBlock()
    {
        auto block = new ComponentDataBlock<T>();

        block->m_Component = new T();
        block->m_ComponentSize = sizeof(T);

        ResizeComponentDataBlock(reinterpret_cast<ComponentDataBlock<IComponent>*>(block), Engine::GetEngineConfiguration().m_MaxObjectCount);

        m_ComponentDataBlocks.push_back(reinterpret_cast<ComponentDataBlock<IComponent>*>(block));

        return block;
    }

    template <class T> std::vector<T>& GetComponentList(ComponentType type)
    {
        // Not going to bother with sanity checking the type, as it's an enum with an already known size.
        // Hopefully all components specified in the enum is also created in this vector though.
        return *m_ComponentDataBlocks[static_cast<int>(type)]->m_ComponentArray;
    }
}

void Pine::Components::Setup()
{
    CreateComponentDataBlock<Transform>();
    CreateComponentDataBlock<SpriteRenderer>();
    CreateComponentDataBlock<TilemapRenderer>();
    CreateComponentDataBlock<Camera>();

    std::size_t totalSize = 0;

    for (auto& block : m_ComponentDataBlocks)
    {
        totalSize += block->m_ComponentArraySize;
    }

    Log::Verbose("Total size allocated for components: " + std::to_string(totalSize / 1024) + " kB (" + std::to_string(Engine::GetEngineConfiguration().m_MaxObjectCount) + " objects per type)");
}

void Pine::Components::Shutdown()
{
    for (const auto block : m_ComponentDataBlocks)
    {
        free(reinterpret_cast<IComponent*>(block->m_ComponentArray));
        delete[] block->m_ComponentOccupationArray;
    }

    m_ComponentDataBlocks.clear();
}

const std::vector<ComponentDataBlock<IComponent>*>& Pine::Components::GetComponentTypes()
{
    return m_ComponentDataBlocks;
}

IComponent* Components::Create(ComponentType type, bool standalone)
{
    auto componentDataBlock = m_ComponentDataBlocks[static_cast<int>(type)];

    IComponent* component;

    // Get a pointer to some free memory for the new component, depending on if we want a standalone
    // or in the data block
    if (standalone)
    {
        component = static_cast<IComponent*>(malloc(componentDataBlock->m_ComponentSize));
    }
    else
    {
        // Find a slot in the array we can use
        const auto newTargetSlot = componentDataBlock->GetAvailableIndex();

        if (newTargetSlot >= componentDataBlock->m_ComponentArrayAllocatedCount)
        {
            // We've run out of space in the array, we need to resize it and make it bigger.
            // Not sure if just putting it + 128 is a good idea, but it's fine for now.
            ResizeComponentDataBlock(componentDataBlock, componentDataBlock->m_ComponentArrayAllocatedCount + 128);
        }

        component = componentDataBlock->GetComponent(newTargetSlot);

        // Mark the index as occupied
        componentDataBlock->m_ComponentOccupationArray[newTargetSlot] = true;

        // Store the new highest index
        componentDataBlock->m_HighestComponentIndex = componentDataBlock->GetHighestComponentIndex();
    }

    // Copy the data from the 'default' component object
    memcpy((void*)component, (void*)componentDataBlock->m_Component, componentDataBlock->m_ComponentSize);

    component->SetStandalone(standalone);
    component->OnCreated();

    return component;
}

IComponent* Components::Copy(IComponent* component, bool standalone)
{
    auto newComponent = Create(component->GetType(), standalone);

    nlohmann::json buffer;

    component->SaveData(buffer);
    newComponent->LoadData(buffer);

    newComponent->OnCopied();

    return newComponent;
}

bool Components::Destroy(IComponent* targetComponent)
{
    if (m_ComponentDataBlocks.empty())
    {
        // If the application is being closed, all the de-constructors in Entity will get called,
        // therefore this method will get called, hence this check.
        return true;
    }

    targetComponent->OnDestroyed();

    // If the component is standalone (i.e. it isn't in the "ECS"), we can just free
    // the memory and move on.
    if (targetComponent->GetStandalone())
    {
        free(targetComponent);

        return true;
    }

    auto& data = GetData(targetComponent->GetType());

    for (std::uint32_t i = 0; i < data.GetHighestComponentIndex();i++)
    {
        auto componentPointer = data.GetComponent(i);

        if (componentPointer == targetComponent)
        {
            // We don't have to free any memory or anything, so marking the slot as "available"
            // should be sufficient.
            data.m_ComponentOccupationArray[i] = false;

            // Store the new highest index
            data.m_HighestComponentIndex = data.GetHighestComponentIndex();

            return true;
        }
    }

    return false;
}

ComponentDataBlock<IComponent>& Components::GetData(ComponentType type)
{
    return *m_ComponentDataBlocks[static_cast<int>(type)];
}
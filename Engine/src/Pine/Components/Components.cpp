#include "Components.hpp"
#include "Pine/Components/Transform/Transform.hpp"
#include "Pine/Core/Log/Log.hpp"

using namespace Pine;

namespace
{
    constexpr int DefaultComponentBlockSize = 512;

    std::vector<ComponentDataBlock<IComponent>*> m_ComponentDataBlocks;

    bool ResizeComponentDataBlock(ComponentDataBlock<IComponent>* block, std::uint32_t size)
    {
        // Allocate the new data
        void* arrayData = malloc(block->m_ComponentArraySize * size);
        bool* arrayOccupationData = new bool[size];

        if (!arrayData)
        {
            Log::Error("Failure allocating data for component block");
            return false;
        }

        memset(arrayData, 0, block->m_ComponentArraySize * size);
        memset(arrayOccupationData, 0, sizeof(bool) * size);

        void* oldArrayData = nullptr;
        void* oldOccupationArray = nullptr;

        // Copy over the old data if we have any
        if (block->m_ComponentArray != nullptr &&
            block->m_ComponentOccupationArray != nullptr)
        {
            size_t arrayBlockCopySize = block->m_ComponentArraySize;
            size_t occupationArrayCopySize = block->m_ComponentOccupationArraySize;

            if (block->m_ComponentArraySize > block->m_ComponentArraySize * size)
            {
                Log::Warning("Smaller new buffer for component data block, data loss possible.");

                arrayBlockCopySize = block->m_ComponentArraySize * size;
                occupationArrayCopySize = sizeof(bool) * size;
            }

            memcpy(arrayData, (void*)block->m_ComponentArray, arrayBlockCopySize);
            memcpy(arrayOccupationData, (void*)block->m_ComponentOccupationArray, occupationArrayCopySize);

            oldArrayData = (void*)block->m_ComponentArray;
            oldOccupationArray = (void*)block->m_ComponentOccupationArray;
        }

        // Set the new array pointers
        block->m_ComponentArray = reinterpret_cast<IComponent*>(arrayData);
        block->m_ComponentArraySize = block->m_ComponentArraySize * size;

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

        block->m_Component = new T(nullptr);

        ResizeComponentDataBlock(reinterpret_cast<ComponentDataBlock<IComponent>*>(block), DefaultComponentBlockSize);

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
}

void Pine::Components::Shutdown()
{
    for (const auto block : m_ComponentDataBlocks)
    {
        free(block->m_ComponentArray);
        free(block->m_ComponentOccupationArray);
    }
}

const std::vector<ComponentDataBlock<IComponent>*>& Pine::Components::GetComponentTypes()
{
    return m_ComponentDataBlocks;
}

IComponent* Components::CreateComponent(ComponentType type, bool standalone)
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
    }

    // Copy the data from the 'default' component object
    memcpy((void*)component, (void*)componentDataBlock->m_Component, componentDataBlock->m_ComponentSize);

    component->SetStandalone(standalone);
    component->OnCreated();

    return component;
}
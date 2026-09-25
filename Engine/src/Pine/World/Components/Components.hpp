#pragma once
#include "Pine/World/Components/Component/Component.hpp"

#include <algorithm>

namespace Pine
{
    template<typename T>
    struct ComponentDataBlockIterator;

    template<typename T>
    struct ComponentDataBlock
    {
        // Pointer to an initialized component object
        // of this block's type.
        T* m_Component = nullptr;
        std::size_t m_ComponentSize = sizeof(T);

        // The allocated data with all component objects
        T* m_ComponentArray = nullptr;
        std::size_t m_ComponentArraySize = 0;

        // Data of which elements of the component array is occupied.
        bool* m_ComponentOccupationArray = nullptr;
        std::size_t m_ComponentOccupationArraySize = 0;

        // The number of components the array currently can fit (capacity)
        std::uint32_t m_ComponentArrayAllocatedCount = 0;

        // The number of slots currently in use
        std::uint32_t m_OccupiedCount = 0;

        // Incrementing counter to hand out unique ids to every created component.
        std::uint64_t m_UniqueIdCount = 0;

        // One past the highest occupied slot, or -1 when the block is empty. Iteration stops here.
        int m_HighestComponentIndex = -1;

        // The lowest free slot, or the capacity when every slot is taken.
        std::uint32_t m_FirstFreeIndex = 0;

        // Sort of hacky, but it allows us to select what components we want to iterate through
        // I don't really like the placing of this either, problem for future me.
        bool m_IterateDisabledObjects = false;

        ComponentDataBlockIterator<T> begin()
        {
            return ComponentDataBlockIterator<T>(0, this, m_IterateDisabledObjects);
        }

        ComponentDataBlockIterator<T> end()
        {
            return ComponentDataBlockIterator<T>(m_HighestComponentIndex == -1 ? 0 : m_HighestComponentIndex, this, m_IterateDisabledObjects);
        }

        // The first free slot, or the capacity when the block is full.
        __inline std::uint32_t GetAvailableIndex() const
        {
            return m_FirstFreeIndex;
        }

        // Marking slots goes through these two so the counts and indices above follow along without
        // rescanning the whole block, which would make every create and destroy cost the capacity.
        void MarkOccupied(const std::uint32_t index)
        {
            m_ComponentOccupationArray[index] = true;
            m_OccupiedCount++;

            m_HighestComponentIndex = std::max(m_HighestComponentIndex, static_cast<int>(index) + 1);

            while (m_FirstFreeIndex < m_ComponentArrayAllocatedCount && m_ComponentOccupationArray[m_FirstFreeIndex])
            {
                m_FirstFreeIndex++;
            }
        }

        void MarkFree(const std::uint32_t index)
        {
            m_ComponentOccupationArray[index] = false;
            m_OccupiedCount--;

            m_FirstFreeIndex = std::min(m_FirstFreeIndex, index);

            // Only freeing the top slot moves the end of iteration, down to the next occupied slot.
            if (static_cast<int>(index) + 1 != m_HighestComponentIndex)
            {
                return;
            }

            int highestIndex = static_cast<int>(index);

            while (highestIndex > 0 && !m_ComponentOccupationArray[highestIndex - 1])
            {
                highestIndex--;
            }

            m_HighestComponentIndex = highestIndex == 0 ? -1 : highestIndex;
        }

        __inline T* GetComponent(const std::uint32_t index)
        {
            // Don't wanna directly access the array here since T could be either
            // an IComponent or the component itself.
            return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(m_ComponentArray) + m_ComponentSize * index);
        }

        __inline bool ComponentIndexValid(const std::uint32_t index) const
        {
            return m_ComponentOccupationArray[index];
        }
    };

    template<typename T>
    struct ComponentDataBlockIterator
    {
    public:
        ComponentDataBlockIterator(uint32_t index, ComponentDataBlock<T>* block, const bool iterateDisabledObjects)
            : m_ComponentIndex(index),
              m_BlockParent(block),
              m_IterateDisabledObjects(iterateDisabledObjects)
        {
            m_ComponentPtr = block->GetComponent(index);

            if (!m_BlockParent->m_ComponentOccupationArray[index] &&
                block->m_HighestComponentIndex != -1 &&
                index < block->m_HighestComponentIndex)
            {
                ++(*this);
            }

            if (!m_IterateDisabledObjects &&
                m_ComponentPtr &&
                block->m_HighestComponentIndex != -1 &&
                index < block->m_HighestComponentIndex)
            {
                if (!reinterpret_cast<Component*>(m_ComponentPtr)->IsWorldEnabled())
                {
                    ++(*this);
                }
            }
        }

        T& operator*() const
        {
            return *m_ComponentPtr;
        }

        T* operator->()
        {
            return m_ComponentPtr;
        }

        ComponentDataBlockIterator& operator++()
        {
            m_ComponentIndex++;

            // If we've reached .end(), we just to stop
            if (m_ComponentIndex == m_BlockParent->m_HighestComponentIndex)
            {
                m_ComponentPtr = m_BlockParent->GetComponent(m_ComponentIndex);
                return *this;
            }

            // Else start searching for the next component
            while (!m_BlockParent->m_ComponentOccupationArray[m_ComponentIndex] ||
                  (!m_IterateDisabledObjects && !reinterpret_cast<Component*>(m_BlockParent->GetComponent(m_ComponentIndex))->IsWorldEnabled()))
            {
                if (m_ComponentIndex >= m_BlockParent->m_HighestComponentIndex)
                {
                    break;
                }

                m_ComponentIndex++;
            }

            m_ComponentPtr = m_BlockParent->GetComponent(m_ComponentIndex);

            return *this;
        }

        ComponentDataBlockIterator<T> operator++(int) // NOLINT(cert-dcl21-cpp)
        {
            ComponentDataBlockIterator<T> tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator== (const ComponentDataBlockIterator& a, const ComponentDataBlockIterator& b) { return a.m_ComponentPtr == b.m_ComponentPtr; };
        friend bool operator!= (const ComponentDataBlockIterator& a, const ComponentDataBlockIterator& b) { return a.m_ComponentPtr != b.m_ComponentPtr; };
    private:
        uint32_t m_ComponentIndex;

        ComponentDataBlock<T>* m_BlockParent;

        T* m_ComponentPtr;

        bool m_IterateDisabledObjects = false;
    };
}

namespace Pine::Components
{
    void Setup();
    void Shutdown();

    // Component Types
    const std::vector<ComponentDataBlock<Component>*>& GetComponentTypes();

    // Creation and deletion of components
    Component* Create(ComponentType type, bool standalone = false);
    Component* Copy(Component* component, bool standalone = false);
    bool Destroy(Component* component);

    // Iteration through component objects
    ComponentDataBlock<Component>& GetData(ComponentType type);

    // Returns a ComponentType from a template type
    template<typename T>
    ComponentType GetType()
    {
        static T ent;
        static auto type = static_cast<Component*>(&ent)->GetType();

        return type;
    }

    template<typename T>
    T& Create()
    {
        static auto type = GetType<T>();

        return *dynamic_cast<T*>(Create(type));
    }

    template<typename T>
    ComponentDataBlock<T>& Get(bool includeInactiveComponents = false)
    {
        static auto type = GetType<T>();

        auto& block = *reinterpret_cast<ComponentDataBlock<T>*>(&GetData(type));

        block.m_IterateDisabledObjects = includeInactiveComponents;

        return block;
    }

    template<typename T>
    T* GetByInternalId(std::uint32_t internalId)
    {
        static auto type = GetType<T>();

        auto& block = *reinterpret_cast<ComponentDataBlock<T>*>(&GetData(type));

        return block.GetComponent(internalId);
    }

    Component* GetByInternalId(ComponentType type, std::uint32_t internalId);

    Component* FindById(ComponentType type, UId id);

    // How many more components of a type fit in its block. Create() throws once it is full, so a
    // caller that cannot let that exception escape checks this first.
    std::uint32_t GetFreeSlotCount(ComponentType type);
}

namespace Pine
{

    template<class T>
    class ComponentHandle
    {
        mutable bool m_Valid = false;

        ComponentType m_Type = ComponentType::Transform;
        UId m_UniqueId{};

        std::uint32_t m_InternalId {};
    public:

        T *Get()
        {
            if (!m_Valid)
            {
                return nullptr;
            }

            const auto& block = Components::GetData(m_Type);
            if (m_InternalId >= block.m_ComponentOccupationArraySize || !block.ComponentIndexValid(m_InternalId))
            {
                m_Valid = false;
                return nullptr;
            }

            const auto component = Components::GetByInternalId(m_Type, m_InternalId);
            if (!component || component->GetId() != m_UniqueId)
            {
                m_Valid = false;
                return nullptr;
            }

            return static_cast<T*>(component);
        }

        T *operator->()
        {
            return Get();
        }

        ComponentHandle &operator=(const Component *component)
        {
            if (!component)
            {
                m_Valid = false;
                return *this;
            }

            m_Type = component->GetType();
            m_UniqueId = component->GetId();
            m_InternalId = component->GetInternalId();
            m_Valid = true;

            return *this;
        }

        bool operator==(const Component *b) const
        {
            return m_UniqueId == b->GetId();
        }
    };

}

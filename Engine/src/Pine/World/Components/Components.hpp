#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"

#include <unordered_map>

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

        // The number of components the array currently can fit
        std::uint32_t m_ComponentArrayAllocatedCount = 0;

        // Cached version of GetHighestComponentIndex(), should always be correct. Will however be -1 if empty.
        int m_HighestComponentIndex = -1;

        // Sort of hacky, but it allows us to select what components we want to iterate through
        bool m_IterateDisabledObjects = false;

        ComponentDataBlockIterator<T> begin()
        {
            return ComponentDataBlockIterator<T>(0, this, m_IterateDisabledObjects);
        }

        ComponentDataBlockIterator<T> end()
        {
            return ComponentDataBlockIterator<T>(m_HighestComponentIndex == -1 ? 0 : m_HighestComponentIndex, this, m_IterateDisabledObjects);
        }

        __inline int GetHighestComponentIndex()
        {
            // This kind of sucks as we're required to loop through the array m_ComponentOccupationArraySize times
            // each time we want to iterate through all components.
            int highestIndex = -1;

            for (int i = 0; i < m_ComponentOccupationArraySize;i++)
            {
                if (m_ComponentOccupationArray[i])
                    highestIndex = i + 1;
            }

            return highestIndex;
        }

        __inline std::uint32_t GetAvailableIndex()
        {
            for (std::uint32_t i = 0; i < m_ComponentOccupationArraySize;i++)
            {
                if (!m_ComponentOccupationArray[i])
                    return i;
            }

            return static_cast<std::uint32_t>(m_ComponentOccupationArraySize);
        }

        __inline T* GetComponent(std::uint32_t index)
        {
            // Don't wanna directly access the array here since T could be either
            // an IComponent or the component itself.
            return reinterpret_cast<T*>(std::uintptr_t(m_ComponentArray) + m_ComponentSize * index);
        }

        __inline bool ComponentIndexValid(std::uint32_t index)
        {
            return m_ComponentOccupationArray[index];
        }
    };

    template<typename T>
    struct ComponentDataBlockIterator
    {
    public:
        ComponentDataBlockIterator(uint32_t index, ComponentDataBlock<T>* block, bool iterateDisabledObjects)
            : m_ComponentIndex(index),
              m_BlockParent(block),
              m_IterateDisabledObjects(iterateDisabledObjects)
        {
            m_ComponentPtr = block->GetComponent(index);

            if (!m_IterateDisabledObjects &&
                m_ComponentPtr &&
                block->m_HighestComponentIndex != -1 &&
                index < block->m_HighestComponentIndex)
            {
                if (!reinterpret_cast<IComponent*>(m_ComponentPtr)->IsWorldEnabled())
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
                  (!m_IterateDisabledObjects && !reinterpret_cast<IComponent*>(m_BlockParent->GetComponent(m_ComponentIndex))->IsWorldEnabled()))
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
    const std::vector<ComponentDataBlock<IComponent>*>& GetComponentTypes();

    // Creation and deletion of components
    IComponent* Create(ComponentType type, bool standalone = false);
    IComponent* Copy(IComponent* component, bool standalone = false);
    bool Destroy(IComponent* component);

    // Iteration through component objects
    ComponentDataBlock<IComponent>& GetData(ComponentType type);

    // Returns a ComponentType from a template type
    template<typename T>
    ComponentType GetType()
    {
        static T ent;
        static auto type = static_cast<IComponent*>(&ent)->GetType();

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

    IComponent* GetByInternalId(Pine::ComponentType type, std::uint32_t internalId);

    // Internal hints that may be set by the engine to optimize component iteration
    void SetIgnoreHighestEntityIndexFlag(bool ignore);

    void RecomputeHighestComponentIndex();
}
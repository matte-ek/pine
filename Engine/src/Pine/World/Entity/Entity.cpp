#include "Entity.hpp"
#include <algorithm>
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{

    struct EntitySerializer : Pine::Serialization::Serializer
    {
        PINE_SERIALIZE_STRING(Name);
        PINE_SERIALIZE_PRIMITIVE(Active, Pine::Serialization::DataType::Boolean);
        PINE_SERIALIZE_PRIMITIVE(Static, Pine::Serialization::DataType::Boolean);
        PINE_SERIALIZE_PRIMITIVE(Tags, Pine::Serialization::DataType::Int64);
        PINE_SERIALIZE_ARRAY(Components);
        PINE_SERIALIZE_ARRAY(Children);
    };

    struct ComponentSerializer : Pine::Serialization::Serializer
    {
        PINE_SERIALIZE_PRIMITIVE(Type, Pine::Serialization::DataType::Int32);
        PINE_SERIALIZE_PRIMITIVE(Active, Pine::Serialization::DataType::Boolean);
        PINE_SERIALIZE_DATA(Data);
    };

}

Pine::Entity::Entity(const UId id)
    : m_Id(id)
{
}

Pine::Entity::Entity(const UId id, const std::uint32_t internalId)
        : m_Id(id), m_InternalId(internalId)
{
    CreateScriptHandle();

    AddComponent<Transform>();
}

Pine::Entity::~Entity()
{
    for (auto& component : m_Components)
    {
        if (!Components::Destroy(component))
        {
            PError("~Entity(): Error destroying component.");
        }

        component = nullptr;
    }

    m_Components.clear();

    // If the id is zero, this entity is not part of the world, therefore we'll have to do things
    // a bit more manually.
    if (m_Id == UId::Empty())
    {
        const auto children = m_Children;

        for (const auto child : children)
        {
            delete child;
        }

        m_Children.clear();
    }
    else
    {
        while (!m_Children.empty())
        {
            Entities::Delete(m_Children.front());
        }
    }

    if (m_Parent != nullptr)
    {
        m_Parent->RemoveChild(this);
    }

    DestroyScriptHandle();
}

Pine::UId Pine::Entity::GetId() const
{
    return m_Id;
}

std::uint32_t Pine::Entity::GetInternalId() const
{
    return m_InternalId;
}

void Pine::Entity::SetActive(const bool value)
{
    m_Active = value;
}

bool Pine::Entity::GetActive() const
{
    return m_Active;
}

void Pine::Entity::SetStatic(const bool value)
{
    m_Static = value;
}

bool Pine::Entity::GetStatic() const
{
    return m_Static;
}

void Pine::Entity::SetTags(const std::uint64_t tags)
{
    m_Tags = tags;
}

std::uint64_t Pine::Entity::GetTags() const
{
    return m_Tags;
}

void Pine::Entity::SetDirty(const bool value)
{
    m_Dirty = value;
}

bool Pine::Entity::IsDirty() const
{
    return m_Dirty;
}

void Pine::Entity::SetTemporary(const bool value)
{
    m_Temporary = value;
}

bool Pine::Entity::GetTemporary() const
{
    return m_Temporary;
}

void Pine::Entity::SetName(const std::string& name)
{
    m_Name = name;
}

const std::string& Pine::Entity::GetName() const
{
    return m_Name;
}

void Pine::Entity::SetParent(Entity* entity)
{
    m_Parent = entity;

    // The world transform is composed from the parent's. An entity that is still being loaded, or
    // is being destroyed, has no Transform to update.
    if (!m_Components.empty())
    {
        GetTransform()->SetDirty();
    }
}

Pine::Entity* Pine::Entity::GetParent() const
{
    return m_Parent;
}

Pine::Component* Pine::Entity::AddComponent(const ComponentType type)
{
    const auto component = Components::Create(type);

    component->SetParent(this);
    component->OnCreated();

    m_Components.push_back(component);

    return component;
}

Pine::Component* Pine::Entity::AddComponent(Component* component)
{
    component->SetParent(this);
    component->OnCreated();

    m_Components.push_back(component);

    return component;
}

bool Pine::Entity::RemoveComponent(const Component* targetComponent)
{
    for (int i = 0; i < m_Components.size();i++)
    {
        const auto component = m_Components[i];

        if (component == targetComponent)
        {
            m_Components.erase(m_Components.begin() + i);

            if (Components::Destroy(component))
                return true;
        }
    }

    return false;
}

void Pine::Entity::ClearComponents()
{
    for (const auto component : m_Components)
    {
        Components::Destroy(component);
    }

    m_Components.clear();
}

Pine::Component * Pine::Entity::GetComponent(const ComponentType type) const
{
    for (auto component : m_Components)
    {
        if (component && component->GetType() == type)
        {
            return component;
        }
    }

    return nullptr;
}

bool Pine::Entity::HasComponent(const ComponentType type) const
{
    return GetComponent(type) != nullptr;
}

Pine::Transform* Pine::Entity::GetTransform() const
{
    if (m_Components.empty())
    {
        throw std::runtime_error("Entity does not contain Transform component");
    }

    // The transform component should always be the first component
    // and should be available in all entities.
    return dynamic_cast<Transform*>(m_Components[0]);
}

const std::vector<Pine::Component*>& Pine::Entity::GetComponents() const
{
    return m_Components;
}

void Pine::Entity::MoveComponent(Component* component, const std::size_t index)
{
    const auto found = std::find(m_Components.begin(), m_Components.end(), component);
    if (found == m_Components.end() || index >= m_Components.size()
        || (component->GetType() == ComponentType::Transform) != (index == 0))
    {
        throw std::runtime_error("Invalid component order: Transform must remain first.");
    }

    m_Components.erase(found);
    m_Components.insert(m_Components.begin() + index, component);
}

Pine::Entity* Pine::Entity::CreateChild()
{
    const auto entity = Entities::Create();

    AddChild(entity);

    return entity;
}

void Pine::Entity::AddChild(Entity* entity)
{
    entity->SetParent(this);

    m_Children.push_back(entity);
}

void Pine::Entity::RemoveChild(Entity* entity)
{
    entity->SetParent(nullptr);

    for (int i = 0; i < m_Children.size();i++)
    {
        if (m_Children[i] == entity)
        {
            m_Children.erase(m_Children.begin() + i);
            break;
        }
    }
}

const std::vector<Pine::Entity*>& Pine::Entity::GetChildren() const
{
    return m_Children;
}

Pine::ByteSpan Pine::Entity::SaveData() const
{
    EntitySerializer entitySerializer;

    entitySerializer.Name.Write(m_Name);
    entitySerializer.Active.Write(m_Active);
    entitySerializer.Static.Write(m_Static);
    entitySerializer.Tags.Write(m_Tags);

    for (const auto component : m_Components)
    {
        ComponentSerializer componentSerializer;

        componentSerializer.Type.Write(component->GetType());
        componentSerializer.Active.Write(component->GetActive());
        componentSerializer.Data.Write(component->SaveData());

        entitySerializer.Components.AddData(componentSerializer.Write());
    }

    for (const auto child : m_Children)
    {
        entitySerializer.Children.AddData(child->SaveData());
    }

    return entitySerializer.Write();
}

void Pine::Entity::LoadData(const ByteSpan& data)
{
    EntitySerializer entitySerializer;

    entitySerializer.Read(data);

    std::string name;
    entitySerializer.Name.Read(name);

    SetName(name);
    SetActive(entitySerializer.Active.Read<bool>());
    SetStatic(entitySerializer.Static.Read<bool>());
    SetTags(entitySerializer.Tags.Read<std::uint64_t>());

    // An entity that belongs to the world was created with a default Transform, and the serialized
    // component list carries its own, so start from an empty entity either way.
    ClearComponents();

    // An entity with no id is not part of the world (see the destructor), so its components and
    // children have to be created detached as well, or they would be registered in a world their
    // owner is not in.
    const bool standalone = m_Id == UId::Empty();

    for (int i = 0; i < entitySerializer.Components.GetDataCount(); i++)
    {
        ComponentSerializer componentSerializer;

        componentSerializer.Read(entitySerializer.Components.GetData(i));

        const auto type = static_cast<ComponentType>(componentSerializer.Type.Read<std::int32_t>());
        const auto component = Components::Create(type, standalone);

        component->LoadData(componentSerializer.Data.Read());

        // The active flag lives next to the component's own data rather than inside it, and
        // files written before it existed simply leave the default in place.
        bool active = true;

        componentSerializer.Active.Read(active);
        component->SetActive(active);

        AddComponent(component);
    }

    for (int i = 0; i < entitySerializer.Children.GetDataCount(); i++)
    {
        const auto child = standalone ? new Entity(UId::Empty()) : Entity::Create();

        AddChild(child);

        child->LoadData(entitySerializer.Children.GetData(i));
    }
}

void Pine::Entity::Delete()
{
    Entities::Delete(this);
}

Pine::Entity* Pine::Entity::Create()
{
    return Entities::Create();
}

Pine::Entity* Pine::Entity::Create(const std::string& name)
{
    return Entities::Create(name);
}

Pine::EntityHandle::EntityHandle() = default;

Pine::EntityHandle::EntityHandle(const Entity* entity)
{
    m_Id = entity->GetId();
    m_InternalId = entity->GetInternalId();
}

Pine::Entity* Pine::EntityHandle::Get()
{
    if (!m_Id.IsValid())
    {
        return nullptr;
    }

    const auto entity = Entities::GetByInternalId(m_InternalId);

    if (entity == nullptr || entity->GetId() != m_Id)
    {
        // As of right now, pine internal entities does not move
        // therefore we won't bother finding the new internal id, as it does not exist.
        m_Id = UId::Empty();
        return nullptr;
    }

    return entity;
}

Pine::Entity* Pine::EntityHandle::operator->()
{
    return Get();
}

Pine::EntityHandle& Pine::EntityHandle::operator=(const Entity* entity)
{
    m_Id = entity->GetId();
    m_InternalId = entity->GetInternalId();

    return *this;
}

Pine::Script::ObjectHandle *Pine::Entity::GetScriptHandle()
{
    return &m_EntityScriptHandle;
}

void Pine::Entity::CreateScriptHandle()
{
    m_EntityScriptHandle = Script::ObjectFactory::CreateEntity(m_Id, m_InternalId);
}

void Pine::Entity::DestroyScriptHandle()
{
    if (!m_EntityScriptHandle.IsValid())
    {
        return;
    }

    Script::ObjectFactory::DisposeEntity(&m_EntityScriptHandle);
}

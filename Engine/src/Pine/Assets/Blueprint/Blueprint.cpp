#include "Blueprint.hpp"
#include "../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/File/File.hpp"

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
        PINE_SERIALIZE_DATA(Data);
    };

    void StoreEntity(Pine::ByteSpan& span, const Pine::Entity* entity)
    {
        EntitySerializer entitySerializer;

        entitySerializer.Name.Write(entity->GetName());
        entitySerializer.Active.Write(entity->GetActive());
        entitySerializer.Static.Write(entity->GetStatic());
        entitySerializer.Tags.Write(entity->GetTags());

        for (auto component : entity->GetComponents())
        {
            ComponentSerializer componentSerializer;

            componentSerializer.Type.Write(component->GetType());
            componentSerializer.Data.Write(component->SaveData());

            entitySerializer.Components.AddData(componentSerializer.Write());
        }

        for (auto child : entity->GetChildren())
        {
            Pine::ByteSpan entityByteSpan;

            StoreEntity(entityByteSpan, child);

            entitySerializer.Children.AddData(entityByteSpan);
        }

        span = entitySerializer.Write();
    }

    void LoadEntity(const Pine::ByteSpan& byteSpan, Pine::Entity* entity)
    {
        EntitySerializer entitySerializer;

        entitySerializer.Read(byteSpan);

        std::string name;
        entitySerializer.Name.Read(name);

        entity->SetName(name);
        entity->SetActive(entitySerializer.Active.Read<bool>());
        entity->SetStatic(entitySerializer.Static.Read<bool>());
        entity->SetTags(entitySerializer.Tags.Read<std::uint64_t>());

        for (int i = 0; i < entitySerializer.Components.GetDataCount();i++)
        {
            ComponentSerializer componentSerializer;

            componentSerializer.Read(entitySerializer.Components.GetData(i));

            auto component = Pine::Components::Create(static_cast<Pine::ComponentType>(componentSerializer.Type.Read<int32_t>()), true);

            component->LoadData(componentSerializer.Data.Read());

            entity->AddComponent(component);
        }

        for (int i = 0; i < entitySerializer.Children.GetDataCount();i++)
        {
            auto child = new Pine::Entity(0);

            entity->AddChild(child);

            LoadEntity(entitySerializer.Children.GetData(i), child);
        }
    }
}

Pine::Blueprint::Blueprint()
{
    m_Type = AssetType::Blueprint;
}

void Pine::Blueprint::CopyEntity(Entity* dst, const Entity* src, bool createInstance)
{
    dst->SetName(src->GetName());
    dst->SetActive(src->GetActive());
    dst->SetStatic(src->GetStatic());

    dst->ClearComponents();

    for (auto component : src->GetComponents())
    {
        if (component->GetType() == ComponentType::NativeScript) // this might be a bad idea.
            continue;

        dst->AddComponent(Components::Copy(component, !createInstance));
    }

    for (auto child : src->GetChildren())
    {
        Entity* newChild;

        if (createInstance)
            newChild = Entity::Create();
        else
            newChild = new Entity(0);

        dst->AddChild(newChild);

        newChild->ClearComponents();

        CopyEntity(newChild, child, createInstance);
    }
}

bool Pine::Blueprint::LoadAssetData(const ByteSpan& span)
{
    FromByteSpan(span);
    return true;
}

Pine::ByteSpan Pine::Blueprint::SaveAssetData()
{
    return ToByteSpan();
}

void Pine::Blueprint::Dispose()
{
    delete m_Entity;
}

bool Pine::Blueprint::HasEntity() const
{
    return m_Entity;
}

void Pine::Blueprint::CreateFromEntity(const Entity* entity)
{
    m_Entity = new Entity(0);

   CopyEntity(m_Entity, entity, false);
}

Pine::Entity* Pine::Blueprint::Spawn() const
{
   if (m_Entity == nullptr)
   {
        throw std::runtime_error("Attempted to spawn invalid blueprint.");
   }

   auto entity = Entity::Create();

   CopyEntity(entity, m_Entity, true);

   return entity;
}

void Pine::Blueprint::FromByteSpan(const ByteSpan& byteSpan)
{
    m_Entity = new Entity(0);

    LoadEntity(byteSpan, m_Entity);
}

Pine::ByteSpan Pine::Blueprint::ToByteSpan() const
{
    if (!m_Entity)
    {
        throw std::runtime_error("Attempted to serialize invalid blueprint.");
    }

    ByteSpan span;

    StoreEntity(span, m_Entity);

    return span;
}

Pine::Entity *Pine::Blueprint::GetEntity() const
{
    return m_Entity;
}

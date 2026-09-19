#include "Blueprint.hpp"
#include "Pine/Core/File/File.hpp"

Pine::Blueprint::Blueprint()
{
    m_Type = AssetType::Blueprint;
}

void Pine::Blueprint::CopyEntity(Entity* dst, const Entity* src, const bool createInstance)
{
    dst->SetName(src->GetName());
    dst->SetActive(src->GetActive());
    dst->SetStatic(src->GetStatic());
    dst->SetTags(src->GetTags());

    dst->ClearComponents();

    for (auto component : src->GetComponents())
    {
        if (component->GetType() == ComponentType::NativeScript) // this might be a bad idea.
            continue;

        const auto copy = Components::Copy(component, !createInstance);
        copy->SetActive(component->GetActive());
        dst->AddComponent(copy);
    }

    for (auto child : src->GetChildren())
    {
        Entity* newChild;

        if (createInstance)
            newChild = Entity::Create();
        else
            newChild = new Entity(UId::Empty());

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
    m_Entity = new Entity(UId::Empty());

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
    m_Entity = new Entity(UId::Empty());

    m_Entity->LoadData(byteSpan);
}

Pine::ByteSpan Pine::Blueprint::ToByteSpan() const
{
    if (!m_Entity)
    {
        throw std::runtime_error("Attempted to serialize invalid blueprint.");
    }

    return m_Entity->SaveData();
}

Pine::Entity *Pine::Blueprint::GetEntity() const
{
    return m_Entity;
}

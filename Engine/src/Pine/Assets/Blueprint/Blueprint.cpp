#include "Blueprint.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

namespace
{

    void StoreEntity(nlohmann::json& j, const Pine::Entity* entity)
    {
        j["name"] = entity->GetName();
        j["active"] = entity->GetActive();
        j["static"] = entity->GetStatic();

        for (auto component : entity->GetComponents())
        {
            nlohmann::json componentJson;

            componentJson["type"] = component->GetType();

            component->SaveData(componentJson);

            j["components"].push_back(componentJson);
        }

        for (auto child : entity->GetChildren())
        {
            nlohmann::json entityJson;

            StoreEntity(entityJson, child);

            j["children"].push_back(entityJson);
        }
    }

    void LoadEntity(const nlohmann::json& j, Pine::Entity* entity)
    {
        entity->SetName(j["name"]);
        entity->SetActive(j["active"]);
        entity->SetStatic(j["static"]);

        for (const auto& componentData : j["components"])
        {
            auto component = Pine::Components::Create(componentData["type"], true);

            component->LoadData(componentData);

            entity->AddComponent(component);
        }

        if (j.contains("children"))
        {
            for (const auto& childData : j["children"])
            {
                auto child = new Pine::Entity(0, false);

                entity->AddChild(child);

                LoadEntity(childData, child);
            }
        }
    }

}

Pine::Blueprint::Blueprint()
{
    m_Type = AssetType::Blueprint;
    m_LoadMode = AssetLoadMode::MultiThread;
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
            newChild = new Entity(0, false);

        dst->AddChild(newChild);

        newChild->ClearComponents();

        CopyEntity(newChild, child, createInstance);
    }
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
    m_Entity = new Entity(0, false);

   CopyEntity(m_Entity, entity, false);

   m_HasBeenModified = true;
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

void Pine::Blueprint::FromJson(const nlohmann::json& j)
{
   m_Entity = new Entity(0, false);

   LoadEntity(j, m_Entity);
}

nlohmann::json Pine::Blueprint::ToJson() const
{
   if (!m_Entity)
   {
        throw std::runtime_error("Attempted to serialize invalid blueprint.");
   }

   nlohmann::json json;

   StoreEntity(json, m_Entity);

   return json;
}

bool Pine::Blueprint::LoadFromFile(AssetLoadStage stage)
{
   auto json = Serialization::LoadFromFile(m_FilePath);

   if (!json.has_value())
   {
        return false;
   }

   FromJson(json.value());

   m_State = AssetState::Loaded;

   return true;
}

bool Pine::Blueprint::SaveToFile()
{
    Serialization::SaveToFile(m_FilePath, ToJson());

    return true;
}

Pine::Entity *Pine::Blueprint::GetEntity() const
{
    return m_Entity;
}

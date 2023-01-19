#include "Blueprint.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

namespace
{

    void StoreEntity(nlohmann::json& j, Pine::Entity* entity)
    {
        j["name"] = entity->GetName();
        j["active"] = entity->GetActive();

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

        for (const auto& componentData : j["components"])
        {
            auto component = Pine::Components::Create(componentData["type"], true);

            component->LoadData(componentData);

            entity->AddComponent(componentData);
        }

        for (const auto& childData : j["children"])
        {
            auto child = new Pine::Entity(0, false);

            entity->AddChild(child);

            LoadEntity(childData, child);
        }
    }

}

Pine::Blueprint::Blueprint()
{
    m_Type = AssetType::Blueprint;
    m_LoadMode = AssetLoadMode::MultiThread;
}

void Pine::Blueprint::CopyEntity(Pine::Entity* dst, const Pine::Entity* src, bool createInstance) const
{
    dst->SetName(src->GetName());
    dst->SetActive(src->GetActive());

    dst->ClearComponents();

    for (auto component : src->GetComponents())
    {
        dst->AddComponent(Components::Copy(component, !createInstance));
    }

    for (auto child : src->GetChildren())
    {
        Pine::Entity* newChild;

        if (createInstance)
            newChild = Entity::Create();
        else
            newChild = new Pine::Entity(0, false);

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

void Pine::Blueprint::CreateFromEntity(Pine::Entity* entity)
{
    m_Entity = new Pine::Entity(0, false);

   CopyEntity(m_Entity, entity, false);
}

void Pine::Blueprint::Spawn()
{
   if (m_Entity == nullptr)
   {
        throw std::runtime_error("Attempted to spawn invalid blueprint.");
   }

   auto entity = Entity::Create();

   CopyEntity(entity, m_Entity, true);
}

void Pine::Blueprint::FromJson(const nlohmann::json& j)
{
   m_Entity = new Pine::Entity(0, false);

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

bool Pine::Blueprint::LoadFromFile(Pine::AssetLoadStage stage)
{
   auto json = Pine::Serialization::LoadFromFile(m_FilePath);

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
    Pine::Serialization::SaveToFile(m_FilePath, ToJson());

    return true;
}
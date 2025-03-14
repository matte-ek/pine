#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <cassert>
#include <unordered_map>
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "ScriptObjectFactory.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "mono/metadata/class.h"

namespace
{
    MonoDomain *m_RootDomain = nullptr;
    MonoAssembly *m_PineAssembly = nullptr;
    MonoImage *m_PineImage = nullptr;

    MonoClass *m_EntityClass = nullptr;
    MonoClassField *m_EntityInternalIdField = nullptr;
    MonoClassField *m_EntityIdProperty = nullptr;
    MonoClassField *m_EntityValidProperty = nullptr;

    struct ComponentTypeData
    {
        MonoClass *m_ComponentClass = nullptr;
        MonoClassField *m_ComponentInternalIdField = nullptr;
        MonoClassField *m_ComponentIsValidField = nullptr;
        MonoClassField *m_ComponentParentField = nullptr;
        MonoClassField *m_ComponentTypeField = nullptr;
    };

    struct AssetTypeData
    {
        MonoClass* m_AssetClass = nullptr;
        MonoClassField* m_AssetInternalIdField = nullptr;
        MonoClassField* m_AssetTypeField = nullptr;
    };

    std::unordered_map<Pine::AssetType, AssetTypeData*> m_AssetObjectFactory;
    std::unordered_map<Pine::ComponentType, ComponentTypeData*> m_ComponentObjectFactory;
}

void Pine::Script::ObjectFactory::Setup()
{
    m_AssetObjectFactory.clear();
    m_ComponentObjectFactory.clear();

    m_RootDomain = Pine::Script::Runtime::GetDomain();
    m_PineAssembly = Pine::Script::Runtime::GetPineAssembly();
    m_PineImage = Pine::Script::Runtime::GetPineImage();

    m_EntityClass = mono_class_from_name(m_PineImage, "Pine.World", "Entity");
    m_EntityInternalIdField = mono_class_get_field_from_name(m_EntityClass, "_internalId");
    m_EntityValidProperty = mono_class_get_field_from_name(m_EntityClass, "_isValid");
    m_EntityIdProperty = mono_class_get_field_from_name(m_EntityClass, "Id");

    assert(m_EntityClass);
    assert(m_EntityInternalIdField);
    assert(m_EntityIdProperty);
    assert(m_EntityValidProperty);
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateEntity(std::uint32_t entityId, std::uint32_t internalId)
{
    if (!m_PineImage)
    {
        return {nullptr, 0};
    }

    auto entity = mono_object_new(m_RootDomain, m_EntityClass);

    mono_runtime_object_init(entity);

    mono_field_set_value(entity, m_EntityInternalIdField, &internalId);
    mono_field_set_value(entity, m_EntityIdProperty, &entityId);

    auto handle = mono_gchandle_new(entity, true);

    return {entity, handle};
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateComponent(const Pine::IComponent *engineComponent)
{
    if (!m_PineImage || engineComponent->GetParent()->GetScriptHandle()->Handle == 0)
    {
        return {nullptr, 0};
    }

    const auto componentType = engineComponent->GetType();
    ComponentTypeData* componentTypeData;

    if (m_ComponentObjectFactory.count(componentType) == 0)
    {
        auto monoClass = mono_class_from_name(m_PineImage, "Pine.World.Components", Pine::ComponentTypeToString(componentType));

        if (!monoClass)
        {
            m_ComponentObjectFactory[componentType] = nullptr;

            return {nullptr, 0};
        }

        componentTypeData = new ComponentTypeData();

        componentTypeData->m_ComponentClass = monoClass;
        componentTypeData->m_ComponentInternalIdField = mono_class_get_field_from_name(componentTypeData->m_ComponentClass, "_internalId");
        componentTypeData->m_ComponentIsValidField = mono_class_get_field_from_name(componentTypeData->m_ComponentClass, "_isValid");
        componentTypeData->m_ComponentParentField = mono_class_get_field_from_name(componentTypeData->m_ComponentClass, "Parent");
        componentTypeData->m_ComponentTypeField = mono_class_get_field_from_name(componentTypeData->m_ComponentClass, "Type");

        m_ComponentObjectFactory[componentType] = componentTypeData;
    }

    componentTypeData = m_ComponentObjectFactory[componentType];

    if (!componentTypeData)
    {
        return {nullptr, 0};
    }

    auto component = mono_object_new(m_RootDomain, componentTypeData->m_ComponentClass);

    mono_runtime_object_init(component);

    auto internalId = engineComponent->GetInternalId();
    auto type = static_cast<int>(engineComponent->GetType());
    
    mono_field_set_value(component, componentTypeData->m_ComponentInternalIdField, &internalId);
    mono_field_set_value(component, componentTypeData->m_ComponentTypeField, &type);
    mono_field_set_value(component, componentTypeData->m_ComponentParentField, mono_gchandle_get_target(engineComponent->GetParent()->GetScriptHandle()->Handle));

    auto handle = mono_gchandle_new(component, true);

    return {component, handle};
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateScriptObject(const Pine::CSharpScript *script, const Pine::IComponent *component)
{
    auto data = script->GetScriptData();
    if (!data || !data->IsReady)
    {
        Log::Warning(fmt::format("Failed to create script object for {}, script data is not ready.", script->GetFileName()));
        return {};
    }

    auto object = mono_object_new(m_RootDomain, data->Class);

    auto internalId = component->GetInternalId();
    auto type = static_cast<int>(component->GetType());

    mono_runtime_object_init(object);
    mono_field_set_value(object, data->ComponentParentField, mono_gchandle_get_target(component->GetParent()->GetScriptHandle()->Handle));
    mono_field_set_value(object, data->ComponentInternalIdField, &internalId);
    mono_field_set_value(object, data->ComponentTypeField, &type);

    auto handle = mono_gchandle_new(object, true);

    return {object, handle};
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateAsset(const Pine::IAsset *asset)
{
    if (!m_PineImage)
    {
        return {nullptr, 0};
    }

    AssetTypeData* assetTypeData;

    if (m_AssetObjectFactory.count(asset->GetType()) == 0)
    {
        auto monoClass = mono_class_from_name(m_PineImage, "Pine.Assets", Pine::AssetTypeToString(asset->GetType()));

        if (!monoClass)
        {
            m_AssetObjectFactory[asset->GetType()] = nullptr;
            
            return { nullptr, 0 };
        }

        assetTypeData = new AssetTypeData();

        assetTypeData->m_AssetClass = monoClass;
        assetTypeData->m_AssetInternalIdField = mono_class_get_field_from_name(monoClass, "_internalId");
        assetTypeData->m_AssetTypeField = mono_class_get_field_from_name(monoClass, "Type");

        m_AssetObjectFactory[asset->GetType()] = assetTypeData;
    }

    assetTypeData = m_AssetObjectFactory[asset->GetType()];

    if (!assetTypeData)
    {
        return { nullptr, 0 };
    }

    auto object = mono_object_new(m_RootDomain, assetTypeData->m_AssetClass);

    mono_runtime_object_init(object);

    auto internalId = asset->GetId();
    auto type = static_cast<int>(asset->GetType());

    mono_field_set_value(object, assetTypeData->m_AssetInternalIdField, &internalId);
    mono_field_set_value(object, assetTypeData->m_AssetTypeField, &type);

    auto handle = mono_gchandle_new(object, true);

    return {object, handle};
}

void Pine::Script::ObjectFactory::DisposeObject(Pine::Script::ObjectHandle *handle)
{
    handle->Object = nullptr;

    mono_gchandle_free(handle->Handle);
}

void Pine::Script::ObjectFactory::DisposeEntity(Pine::Script::ObjectHandle *handle)
{
    int newValidState = 0;

    mono_field_set_value(mono_gchandle_get_target(handle->Handle), m_EntityValidProperty, &newValidState);

    DisposeObject(handle);
}

void Pine::Script::ObjectFactory::DisposeComponent(const IComponent* component, Pine::Script::ObjectHandle *handle)
{
    const auto& componentData = m_ComponentObjectFactory[component->GetType()];

    bool newValidState = false;

    mono_field_set_value(mono_gchandle_get_target(handle->Handle), componentData->m_ComponentIsValidField, &newValidState);

    DisposeObject(handle);
}
